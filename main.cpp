#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>
#include <plog/Severity.h>
#include <rtc/candidate.hpp>
#include <rtc/common.hpp>
#include <rtc/configuration.hpp>
#include <rtc/datachannel.hpp>
#include <rtc/description.hpp>
#include <rtc/global.hpp>
#include <rtc/peerconnection.hpp>
#include <string>
#include <variant>
#include <vector>

constexpr bool IS_OFFERER = false;

int
main()
{
  plog::Severity plog_severity = plog::info;
  plog::ColorConsoleAppender<plog::TxtFormatter> plog_console_appender;
  plog::init(plog_severity, &plog_console_appender);

  rtc::InitLogger(static_cast<rtc::LogLevel>(plog::warning));

  if (IS_OFFERER) {
    std::cout << "-- OFFERER --\n\n";
  } else {
    std::cout << "-- ANSWERER --\n\n";
  }

  /* Configuration */

  rtc::Configuration rtc_cfg;

  rtc_cfg.iceServers.emplace_back("stun.l.google.com:19302");
  rtc_cfg.iceServers.emplace_back("stun1.l.google.com:19302");
  rtc_cfg.iceServers.emplace_back("stun2.l.google.com:19302");
  rtc_cfg.iceServers.emplace_back("stun3.l.google.com:19302");
  rtc_cfg.iceServers.emplace_back("stun4.l.google.com:19302");

  /* PeerConnection */

  rtc::PeerConnection rtc_pc(rtc_cfg);

  std::optional<rtc::Description> rtc_sdp;
  std::optional<rtc::Candidate> ice_cand;

  rtc_pc.onLocalDescription([&](const rtc::Description& x) { rtc_sdp = x; });
  rtc_pc.onLocalCandidate([&](const rtc::Candidate& x) { ice_cand = x; });

  /* DataChannel */

  std::shared_ptr<rtc::DataChannel> rtc_dc;

  if (IS_OFFERER) {
    rtc_dc = rtc_pc.createDataChannel("DataChannel0");
  } else {
    rtc_pc.onDataChannel(
      [&](std::shared_ptr<rtc::DataChannel> x) { rtc_dc = x; });
  }

  /* Present */

  if (IS_OFFERER) {
    while (!rtc_sdp.has_value() || !ice_cand.has_value()) {
      __noop;
    }

    std::cout << std::format(
      "{}\n{}\n{}\n{}\n\n",
      R"([Copy LOCAL DESCRIPTION (paste this to the other peer):])",
      std::string(rtc_sdp.value_or(rtc::Description("internal error"))),
      R"([Copy LOCAL CANDIDATE (paste this to the other peer):])",
      std::string(ice_cand.value_or(rtc::Candidate("internal error"))));
  }

enter_remote_description: {
  std::string str;
  std::cout << "[Enter REMOTE DESCRIPTION:]" << '\n';
  for (std::string l; std::getline(std::cin, l) && !l.empty();) {
    str += l + '\n';
  }
  try {
    rtc_pc.setRemoteDescription(str);
  } catch (const std::exception& e) {
    LOGE << e.what();
    goto enter_remote_description;
  }
}

enter_remote_candidate: {
  std::string str;
  std::cout << "[Enter REMOTE CANDIDATE:]" << '\n';
  std::getline(std::cin, str);
  try {
    rtc_pc.addRemoteCandidate(str);
  } catch (const std::exception& e) {
    LOGE << e.what();
    goto enter_remote_candidate;
  }
}

  if (!IS_OFFERER) {
    while (!rtc_sdp.has_value() || !ice_cand.has_value()) {
      __noop;
    }

    std::cout << std::format(
      "\n{}\n{}\n{}\n{}\n\n",
      R"([Copy LOCAL DESCRIPTION (paste this to the other peer):])",
      std::string(rtc_sdp.value_or(rtc::Description("internal error"))),
      R"([Copy LOCAL CANDIDATE (paste this to the other peer):])",
      std::string(ice_cand.value_or(rtc::Candidate("internal error"))));
  }

  while (rtc_dc == nullptr) {
    __noop;
  }

  rtc_dc->onMessage([](rtc::message_variant data) {
    if (std::holds_alternative<std::string>(data)) {
      std::cout << std::format("[Received MESSAGE: {}]\n",
                               std::get<std::string>(data));
    }
  });

  while (!rtc_dc->isOpen()) {
    __noop;
  }

send_message: {
  std::string str;
  std::cout << "[Send MESSAGE:]" << '\n';
  std::getline(std::cin, str);
  try {
    rtc_dc->send(str);
  } catch (const std::exception& e) {
    LOGE << e.what();
    goto send_message;
  }
}

  goto send_message;

  return EXIT_SUCCESS;
}