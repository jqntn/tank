#include <chrono>
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
#include <thread>
#include <variant>
#include <vector>

constexpr bool IS_OFFERER = true;

constexpr std::chrono::duration DEFAULT_WAIT_DURATION =
  std::chrono::milliseconds(10);

std::optional<rtc::Description> g_local_description;
std::optional<rtc::Candidate> g_local_candidate;

static void
wait_and_print_local_invitation()
{
  while (!g_local_description.has_value()) {
    std::this_thread::sleep_for(DEFAULT_WAIT_DURATION);
  }

  while (!g_local_candidate.has_value()) {
    std::this_thread::sleep_for(DEFAULT_WAIT_DURATION);
  }

  std::cout << std::format("{}\n{}\n{}\n",
                           R"([Copy LOCAL INVITATION:])",
                           std::string(g_local_candidate.value()),
                           std::string(g_local_description.value()));
}

static void
send_message(std::shared_ptr<rtc::DataChannel> rtc_dc)
{
  std::string str;
  std::getline(std::cin, str);

  if (!str.empty()) {
    try {
      rtc_dc->send(str);
    } catch (const std::exception& e) {
      LOGE << e.what();
    }
  }

  send_message(rtc_dc);
}

static void
parse_remote_invitation(rtc::PeerConnection& rtc_pc)
{
  std::cout << "[Paste REMOTE INVITATION:]" << '\n';

  std::string remote_description;
  std::string remote_candidate;
  std::getline(std::cin, remote_candidate);
  for (std::string l; std::getline(std::cin, l) && !l.empty();) {
    remote_description += l + '\n';
  }

  try {
    rtc_pc.setRemoteDescription(remote_description);
    rtc_pc.addRemoteCandidate(remote_candidate);
  } catch (const std::exception& e) {
    LOGE << e.what();
    parse_remote_invitation(rtc_pc);
  }
}

static void
wait_and_config_data_channel(const std::shared_ptr<rtc::DataChannel>& rtc_dc)
{
  while (rtc_dc == nullptr) {
    std::this_thread::sleep_for(DEFAULT_WAIT_DURATION);
  }

  rtc_dc->onMessage([rtc_dc](rtc::message_variant data) {
    if (std::holds_alternative<std::string>(data)) {
      std::cout << std::format("[Received MESSAGE: {}]\n",
                               std::get<std::string>(data));
    }
  });
}

int
main()
{
  plog::Severity plog_severity = plog::info;
  plog::ColorConsoleAppender<plog::TxtFormatter> plog_console_appender;
  plog::init(plog_severity, &plog_console_appender);

  rtc::InitLogger(static_cast<rtc::LogLevel>(plog::warning));

  if (IS_OFFERER) {
    std::cout << "-- OFFERER --" << '\n' << '\n';
  } else {
    std::cout << "-- ANSWERER --" << '\n' << '\n';
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

  rtc_pc.onLocalDescription(
    [&](const rtc::Description& x) { g_local_description = x; });
  rtc_pc.onLocalCandidate([&](const rtc::Candidate& x) {
    if (x.type() == rtc::Candidate::Type::Relayed) {
      LOGF << "Symmetric NAT not supported";
      std::abort();
    } else if (x.type() == rtc::Candidate::Type::ServerReflexive) {
      g_local_candidate = x;
    }
  });

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
    wait_and_print_local_invitation();
  }

  parse_remote_invitation(rtc_pc);

  if (!IS_OFFERER) {
    wait_and_print_local_invitation();
  }

  wait_and_config_data_channel(rtc_dc);

  std::cout << "[Send MESSAGES:] " << '\n';

  send_message(rtc_dc);

  return EXIT_SUCCESS;
}