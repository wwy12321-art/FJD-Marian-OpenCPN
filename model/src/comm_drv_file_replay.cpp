/***************************************************************************
 *   Copyright (C) 2024 by Your Name                                       *
 *   Modified: 2025-10-30 18:30 - File replay driver implementation
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <https://www.gnu.org/licenses/>. *
 **************************************************************************/

/**
 * \file
 *
 * Implement comm_drv_file_replay.h -- driver for replaying files
 */

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/tokenzr.h>
#include <wx/filename.h>
#include <fstream>
#include <sstream>
#include <ctime>

#include "model/comm_drv_file_replay.h"
#include "model/comm_navmsg.h"
#include "model/ocpn_utils.h"

// Replay thread class
class ReplayThread : public wxThread {
public:
  ReplayThread(FileReplayCommDriver* driver) : wxThread(wxTHREAD_JOINABLE), m_driver(driver) {}

  virtual ExitCode Entry() override {
    if (m_driver) {
      m_driver->ReplayThreadMain();
    }
    return (ExitCode)0;
  }

private:
  FileReplayCommDriver* m_driver;
};

FileReplayCommDriver::FileReplayCommDriver(const std::string& file_path,
                                           int replay_speed, bool loop)
    : AbstractCommDriver(NavAddr::Bus::N0183, "FileReplay"),
      m_file_path(file_path),
      m_replay_speed(replay_speed),
      m_loop(loop),
      m_replaying(false),
      m_paused(false),
      m_initial_delay(0),
      m_current_index(0),
      m_replay_thread(nullptr),
      m_listener(nullptr) {
  LoadFile();
}

FileReplayCommDriver::~FileReplayCommDriver() {
  StopReplay();
}

bool FileReplayCommDriver::LoadFile() {
  m_data_lines.clear();

  wxLogMessage("File replay: Attempting to load file: %s", m_file_path.c_str());

  if (!wxFileName::FileExists(m_file_path)) {
    wxLogError("File replay: File not found: %s", m_file_path.c_str());
    return false;
  }

  std::ifstream file(m_file_path);
  if (!file.is_open()) {
    wxLogError("File replay: Cannot open file: %s", m_file_path.c_str());
    return false;
  }
  
  wxLogMessage("File replay: File opened successfully");

  double base_timestamp = -1;
  std::string line;
  int line_count = 0;
  bool first_line = true;

  while (std::getline(file, line)) {
    line_count++;
    if (line.empty() || line[0] == '#') continue;  // Skip empty lines and comments
    
    // Remove UTF-8 BOM if present (especially on first line)
    if (first_line && line.length() >= 3) {
      // UTF-8 BOM is EF BB BF (3 bytes)
      if ((unsigned char)line[0] == 0xEF && 
          (unsigned char)line[1] == 0xBB && 
          (unsigned char)line[2] == 0xBF) {
        line = line.substr(3);  // Skip BOM
        wxLogMessage("File replay: Skipped UTF-8 BOM");
      }
      first_line = false;
    }
    
    // Also trim any leading whitespace or invisible characters
    size_t start = line.find_first_of("!$");
    if (start != std::string::npos && start > 0) {
      line = line.substr(start);
    }

    DataLine data;
    if (ParseLine(line, data)) {
      // If this is the first valid line, set base timestamp
      if (base_timestamp < 0) {
        base_timestamp = data.timestamp_offset;
      }
      // Convert to offset from start
      data.timestamp_offset -= base_timestamp;
      m_data_lines.push_back(data);
    }
  }

  file.close();
  wxLogMessage("File replay: Loaded %zu sentences from %s", m_data_lines.size(),
               m_file_path.c_str());
  return !m_data_lines.empty();
}

bool FileReplayCommDriver::ParseLine(const std::string& line, DataLine& data) {
  // Try to parse timestamp if present
  // Format 1: "!AIVDM,1,1,,B,169<lFOP008g7@`A:;iP@wv20818,0*16    2016/5/5 8:55:19"
  // Format 2: "YYYY-MM-DD HH:MM:SS,sentence" or "timestamp_seconds,sentence"
  // Format 3: "sentence" (no timestamp, use sequential timing)

  // Check for Format 1: sentence followed by spaces and date/time
  // Look for the last colon (part of time HH:MM:SS)
  size_t last_colon = line.rfind(':');
  if (last_colon != std::string::npos && last_colon < line.length() - 2) {
    // Find the start of the timestamp by looking backwards for multiple spaces
    size_t timestamp_start = last_colon;
    while (timestamp_start > 0 && line[timestamp_start] != ' ') {
      timestamp_start--;
    }
    // Continue backwards through spaces
    while (timestamp_start > 0 && line[timestamp_start] == ' ') {
      timestamp_start--;
    }
    
    if (timestamp_start > 10 && timestamp_start < line.length() - 15) {
      timestamp_start++; // Move to first space
      
      std::string sentence_part = line.substr(0, timestamp_start);
      std::string timestamp_str = line.substr(timestamp_start);
      
      // Trim spaces
      size_t first_non_space = timestamp_str.find_first_not_of(" ");
      if (first_non_space != std::string::npos) {
        timestamp_str = timestamp_str.substr(first_non_space);
      }
      
      size_t last_non_space = sentence_part.find_last_not_of(" \t\r\n");
      if (last_non_space != std::string::npos) {
        sentence_part = sentence_part.substr(0, last_non_space + 1);
      }

      // Try to parse "YYYY/M/D H:MM:SS" format
      if (timestamp_str.length() >= 10) {
        int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
        int parsed = sscanf(timestamp_str.c_str(), "%d/%d/%d %d:%d:%d", 
                   &year, &month, &day, &hour, &min, &sec);
        
        if (parsed == 6 && year >= 1900 && year <= 2100 && 
            month >= 1 && month <= 12 && day >= 1 && day <= 31) {
          // Convert to Unix timestamp
          struct tm tm_time = {0};
          tm_time.tm_year = year - 1900;
          tm_time.tm_mon = month - 1;
          tm_time.tm_mday = day;
          tm_time.tm_hour = hour;
          tm_time.tm_min = min;
          tm_time.tm_sec = sec;
          tm_time.tm_isdst = -1; // Let mktime determine DST
          
          time_t timestamp = mktime(&tm_time);
          if (timestamp != -1) {
            data.timestamp_offset = static_cast<double>(timestamp);
            data.sentence = sentence_part;
            return true;
          }
        }
      }
    }
  }

  // Format 2: Try comma-separated format "timestamp,sentence"
  size_t comma_pos = line.find(',');
  if (comma_pos != std::string::npos && comma_pos > 0 && comma_pos < 30) {
    std::string timestamp_str = line.substr(0, comma_pos);
    std::string sentence_part = line.substr(comma_pos + 1);

    // Try to parse as number (Unix timestamp or seconds)
    char* end;
    double timestamp = strtod(timestamp_str.c_str(), &end);
    if (end != timestamp_str.c_str() && *end == '\0') {
      // Successfully parsed as number
      data.timestamp_offset = timestamp;
      data.sentence = sentence_part;
      return true;
    }
  }

  // Format 3: No timestamp or failed to parse, use sequential timing (1 second per line)
  static int line_index = 0;
  data.timestamp_offset = line_index++;
  data.sentence = line;
  return true;
}

bool FileReplayCommDriver::StartReplay() {
  wxLogMessage("File replay: StartReplay called, m_replaying=%d, data_lines=%zu", 
               m_replaying, m_data_lines.size());
  
  if (m_replaying) {
    wxLogMessage("File replay: Already replaying");
    return true;
  }
  
  if (m_data_lines.empty()) {
    wxLogError("File replay: No data to replay");
    return false;
  }

  if (!m_listener) {
    wxLogWarning("File replay: No listener set, delaying start");
    return false;
  }

  m_replaying = true;
  m_paused = false;
  m_current_index = 0;

  m_replay_thread = new ReplayThread(this);
  if (m_replay_thread->Create() != wxTHREAD_NO_ERROR ||
      m_replay_thread->Run() != wxTHREAD_NO_ERROR) {
    wxLogError("File replay: Failed to start replay thread");
    delete m_replay_thread;
    m_replay_thread = nullptr;
    m_replaying = false;
    return false;
  }

  wxLogMessage("File replay: Started replay from %s with %zu sentences", 
               m_file_path.c_str(), m_data_lines.size());
  return true;
}

void FileReplayCommDriver::StopReplay() {
  if (!m_replaying) return;

  m_replaying = false;

  if (m_replay_thread) {
    // Wait for thread to finish
    m_replay_thread->Wait();
    delete m_replay_thread;
    m_replay_thread = nullptr;
  }

  wxLogMessage("File replay: Stopped");
}

void FileReplayCommDriver::SetPaused(bool paused) {file:///home/carl/%E4%B8%8B%E8%BD%BD/20240505085311.txt

  wxCriticalSectionLocker lock(m_critical);
  m_paused = paused;
  wxLogMessage("File replay: %s", paused ? "Paused" : "Resumed");
}

void FileReplayCommDriver::SetReplaySpeed(int speed) {
  if (speed < 1) speed = 1;
  if (speed > 100) speed = 100;

  wxCriticalSectionLocker lock(m_critical);
  m_replay_speed = speed;
  wxLogMessage("File replay: Speed set to %dx", speed);
}

void FileReplayCommDriver::ReplayThreadMain() {
  wxLogMessage("File replay: Thread started");

  // Initial delay to allow system to stabilize
  if (m_initial_delay > 0) {
    wxLogMessage("File replay: Waiting %d ms before sending messages", m_initial_delay);
    wxThread::Sleep(m_initial_delay);
    wxLogMessage("File replay: Initial delay complete, starting replay");
  }

  while (m_replaying) {
    // Check if paused
    {
      wxCriticalSectionLocker lock(m_critical);
      if (m_paused) {
        wxThread::Sleep(100);
        continue;
      }
    }

    // Check if we've reached the end
    if (m_current_index >= m_data_lines.size()) {
      if (m_loop) {
        wxLogMessage("File replay: Looping");
        m_current_index = 0;
      } else {
        wxLogMessage("File replay: Reached end of file");
        m_replaying = false;
        break;
      }
    }

    // Get current and next data
    const DataLine& current = m_data_lines[m_current_index];

    // Send the message
    if (m_listener) {
      std::string sentence = current.sentence;
      
      // Basic validation: must start with $ or ! and be at least 6 chars
      if (sentence.empty() || sentence.length() < 6) {
        wxLogWarning("File replay: Sentence too short, skipping");
        m_current_index++;
        continue;
      }
      
      if (sentence[0] != '$' && sentence[0] != '!') {
        wxLogWarning("File replay: Invalid sentence start: %s", sentence.c_str());
        m_current_index++;
        continue;
      }
      
      // Extract message ID (5 characters after $ or !)
      std::string msg_id = sentence.substr(1, 5);
      
      // Create and send message
      try {
        auto src = std::make_shared<NavAddr0183>("FileReplay");
        auto msg = std::make_shared<const Nmea0183Msg>(msg_id, sentence, src, NavMsg::State::kOk);
        
        if (m_listener && msg && msg->source && !msg->source->iface.empty()) {
          m_listener->Notify(std::move(msg));
        }
      } catch (const std::exception& e) {
        wxLogError("File replay: Exception sending message #%zu: %s", m_current_index, e.what());
      }
    }

    // Calculate delay until next message  
    double delay_ms = 100;  // Default delay
    if (m_current_index + 1 < m_data_lines.size()) {
      const DataLine& next = m_data_lines[m_current_index + 1];
      double time_diff = next.timestamp_offset - current.timestamp_offset;
      
      // Apply replay speed
      int speed;
      {
        wxCriticalSectionLocker lock(m_critical);
        speed = m_replay_speed;
      }
      
      if (speed > 0 && time_diff > 0) {
        delay_ms = (time_diff * 1000.0) / speed;
        // Limit delay to reasonable range
        if (delay_ms < 10) delay_ms = 10;      // Minimum 10ms
        if (delay_ms > 5000) delay_ms = 5000;  // Maximum 5 seconds
      }
    }

    // Move to next message (ONLY ONCE!)
    m_current_index++;

    // Sleep before next message
    wxThread::Sleep((unsigned long)delay_ms);
  }

  wxLogMessage("File replay: Thread exiting");
}

void FileReplayCommDriver::SetListener(DriverListener& listener) {
  wxLogMessage("File replay: SetListener called");
  m_listener = &listener;
  // Do NOT auto-start replay here - must wait for delayed activation
  wxLogMessage("File replay: Listener set (replay will be started after delayed activation)");
}

bool FileReplayCommDriver::SendMessage(std::shared_ptr<const NavMsg> msg,
                                       std::shared_ptr<const NavAddr> addr) {
  // File replay is read-only
  return false;
}

