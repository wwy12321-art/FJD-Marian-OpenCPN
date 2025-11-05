/***************************************************************************
 *   Copyright (C) 2024 by Your Name                                       *
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
 * Comm driver for replaying NMEA data from file with timing control
 */

#ifndef COMM_DRV_FILE_REPLAY_H
#define COMM_DRV_FILE_REPLAY_H

#include <wx/thread.h>
#include <string>
#include <vector>

#include "model/comm_driver.h"

// Forward declaration
class ReplayThread;

/**
 * Communication driver for file replay with speed control and looping.
 * Supports NMEA0183 sentences with optional timestamps.
 * File format: One sentence per line, optionally with timestamp prefix
 * Example: "2024-01-01 12:00:00,!AIVDM,1,1,,B,13aEOK?P00PD2wVMdLDRhgvL289?,0*26"
 * or just: "!AIVDM,1,1,,B,13aEOK?P00PD2wVMdLDRhgvL289?,0*26"
 */
class FileReplayCommDriver : public AbstractCommDriver {
  friend class ReplayThread;  // Allow ReplayThread to access private methods
  
public:
  FileReplayCommDriver(const std::string& file_path, int replay_speed = 1,
                       bool loop = false);
  virtual ~FileReplayCommDriver();

  /** Start replay thread */
  bool StartReplay();

  /** Stop replay thread */
  void StopReplay();

  /** Pause/Resume replay */
  void SetPaused(bool paused);

  /** Set replay speed multiplier (1 = real-time) */
  void SetReplaySpeed(int speed);

  /** Set looping mode */
  void SetLoop(bool loop) { m_loop = loop; }
  
  /** Set initial delay before starting to send messages (milliseconds) */
  void SetInitialDelay(int delay_ms) { m_initial_delay = delay_ms; }

  /** Get current replay status */
  bool IsReplaying() const { return m_replaying; }
  bool IsPaused() const { return m_paused; }

  // AbstractCommDriver interface
  void SetListener(DriverListener& listener) override;
  bool SendMessage(std::shared_ptr<const NavMsg> msg,
                   std::shared_ptr<const NavAddr> addr) override;

private:
  struct DataLine {
    std::string sentence;
    double timestamp_offset;  // Seconds from start
  };

  /** Replay thread main function */
  void ReplayThreadMain();

  /** Load file and parse timestamps */
  bool LoadFile();

  /** Parse a line from file */
  bool ParseLine(const std::string& line, DataLine& data);

  std::string m_file_path;
  int m_replay_speed;
  bool m_loop;
  bool m_replaying;
  bool m_paused;
  int m_initial_delay;  // Initial delay before sending messages (ms)

  std::vector<DataLine> m_data_lines;
  size_t m_current_index;

  wxThread* m_replay_thread;
  wxCriticalSection m_critical;

  DriverListener* m_listener;
};

#endif  // COMM_DRV_FILE_REPLAY_H

