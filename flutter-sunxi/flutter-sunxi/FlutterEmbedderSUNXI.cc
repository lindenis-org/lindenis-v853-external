// Copyright 2021 The Allwinnertech. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

#include <embedder.h>
#include <unistd.h>
#include <getopt.h>
#include "evdev.h"
#ifdef USE_GLES3
#include "sunxiegl.h"
#else
#include "sunxifb.h"
#endif
#include "task_runner.h"
#include "system_utils.h"

#define DEBUG_MSG(str) do \
  { std::cout << "flutter: " << str << std::endl; } \
  while( false )

#ifdef USE_GLES3
#define APP_NAME "flutter_eglfs"
#else
#define APP_NAME "flutter_fbdev"
#endif
#define APP_VERSION "1.0.6"

bool fps_print = false;
bool touch_print = false;
bool swap_touch_xy = false;
bool flip_touch_x = false;
bool flip_touch_y = false;
std::string bundle_path;

// The handle to the embedder.h engine instance.
FLUTTER_API_SYMBOL(FlutterEngine) engine_ = nullptr;
FlutterEngineProcTable embedder_api_;

static void print_usage() {
  std::cout << APP_NAME << " - run flutter apps on your device." << std::endl;
  std::cout << std::endl;

  std::cout << "USAGE:" << std::endl;
  std::cout << "  " << APP_NAME << " [options] <bundle path>" << std::endl;
  std::cout << std::endl;

  std::cout << "OPTIONS:" << std::endl;
  std::cout << "  -f, --fps-print      Print frame rates." << std::endl;
  std::cout << "  -p, --touch-print    Print touch points." << std::endl;
  std::cout << "  -s, --swap-touch     Swap the values of touch x and y."
      << std::endl;
  std::cout << "  -x, --flip-x         Flip x coordinate." << std::endl;
  std::cout << "  -y, --flip-y         Flip y coordinate." << std::endl;
  std::cout << "  -v, --version        Show " << APP_NAME
      << " version and exit." << std::endl;
  std::cout << "  -h, --help           Show this help and exit." << std::endl;
  std::cout << std::endl;

  std::cout << "BUNDLE PATH TREE:" << std::endl;
  std::cout << "  ./app_bundle/data/flutter_assets" << std::endl;
  std::cout << "  ./app_bundle/data/icudtl.dat" << std::endl;
  std::cout << "  ./app_bundle/lib/libapp.so" << std::endl;
  std::cout << std::endl;

  //std::cout << "ENGINE OPTIONS:" << std::endl;
  //std::cout << "  The options of flutter engine can be accessed:" << std::endl;
  //std::cout
  //    << "    https://github.com/flutter/engine/blob/master/shell/common/switches.h"
  //    << std::endl;
  //std::cout << std::endl;

  std::cout << "EXAMPLES:" << std::endl;
  std::cout << "  " << APP_NAME << " ./app_bundle" << std::endl;
  std::cout << "  " << APP_NAME << " -s ./app_bundle" << std::endl;
  std::cout << "  LD_LIBRARY_PATH=./ " << APP_NAME << " ./app_bundle"
      << std::endl;
  std::cout
      << "    LD_LIBRARY_PATH can ensure that libflutter_engine.so is found."
      << std::endl;
  std::cout << std::endl;

  std::cout << "OTHER:" << std::endl;
  std::cout << "  Some applications may require system information." << std::endl;
  std::cout << "  export LANG=\"en_US.UTF-8\"" << std::endl;
}

static bool parse_cmd_args(int argc, char **argv) {
  int opt;
  int longopt_index = 0;
  bool finished_parsing_options = false;

  struct option long_options[] = { { "fps-print", no_argument, 0, 'f' }, {
      "touch-print", no_argument, 0, 'p' },
      { "swap-touch", no_argument, 0, 's' }, { "flip-x", no_argument, 0, 'x' },
      { "flip-y", no_argument, 0, 'y' }, { "version", no_argument, 0, 'v' }, {
          "help", no_argument, 0, 'h' }, { 0, 0, 0, 0 } };

  while (!finished_parsing_options) {
    longopt_index = 0;
    opt = getopt_long(argc, argv, "fpsxyvh", long_options, &longopt_index);

    switch (opt) {
    case 'f':
      fps_print = true;
      break;
    case 'p':
      touch_print = true;
      break;
    case 's':
      swap_touch_xy = true;
      break;
    case 'x':
      flip_touch_x = true;
      break;
    case 'y':
      flip_touch_y = true;
      break;
    case 'v':
      std::cout
          << "Flutter 2.5.1 ? channel stable ? https://github.com/flutter/flutter.git"
          << std::endl;
      std::cout
          << "Framework ? revision ffb2ecea52 (4 days ago) ? 2021-09-17 15:26:33 -0400"
          << std::endl;
      std::cout << "Engine ? revision b3af521a05" << std::endl;
      std::cout << "Tools ? Dart 2.14.2" << std::endl;
      std::cout << "Client ? " << APP_NAME << " " << APP_VERSION << std::endl;
      return false;
    case 'h':
      print_usage();
      return false;
    case '?':
    case ':':
      print_usage();
      return false;
    case -1:
      finished_parsing_options = true;
      break;
    default:
      break;
    }
  }

  if (optind >= argc) {
    DEBUG_MSG("Need to set the bundle path.");
    print_usage();
    return false;
  }

  bundle_path = argv[optind];

  return true;
}

int main(int argc, char **argv) {
  if (!parse_cmd_args(argc, argv)) {
    return 0;
  }

  //Can only run on the sunxi platform
  if (access("/sys/class/sunxi_info/sys_info", F_OK) != 0)
    return 0;

  uint32_t width, height;

#ifdef USE_GLES3
  sunxiegl_init();
  sunxiegl_get_sizes(&width, &height);
#else
  sunxifb_init(0);
  sunxifb_get_sizes(&width, &height);
#endif /* USE_GLES3 */
  evdev_init();

  std::string assets_path = bundle_path + "/data/flutter_assets";
  std::string icu_data_path = bundle_path + "/data/icudtl.dat";
  std::string elf_path = bundle_path + "/lib/libapp.so";

  FlutterEngineResult result;
  FlutterEngineAOTDataSource source = { };
  source.type = kFlutterEngineAOTDataSourceTypeElfPath;
  source.elf_path = elf_path.c_str();
  FlutterEngineAOTData data = nullptr;

  embedder_api_.struct_size = sizeof(FlutterEngineProcTable);
  FlutterEngineGetProcAddresses(&embedder_api_);

  if (embedder_api_.RunsAOTCompiledDartCode()) {
    result = embedder_api_.CreateAOTData(&source, &data);
    if (result != kSuccess) {
      DEBUG_MSG("Failed to load AOT data from: " << elf_path);
      return 0;
    }
  }

  // Task runner for tasks posted from the engine.
  std::unique_ptr<flutter::TaskRunner> task_runner_ = std::make_unique
      < flutter::TaskRunner
      > (std::this_thread::get_id(), embedder_api_.GetCurrentTime, [](
          const auto *task) {
        if (!engine_) {
          DEBUG_MSG("Cannot post an engine task when engine is not running.");
          return;
        }
        if (embedder_api_.RunTask(engine_, task) != kSuccess) {
          DEBUG_MSG("Failed to post an engine task.");
        }
      });
  // Configure task runners.
  FlutterTaskRunnerDescription platform_task_runner = { };
  platform_task_runner.struct_size = sizeof(FlutterTaskRunnerDescription);
  platform_task_runner.user_data = task_runner_.get();
  platform_task_runner.runs_task_on_current_thread_callback =
      [](void *user_data) -> bool {
        return static_cast<flutter::TaskRunner*>(user_data)->RunsTasksOnCurrentThread();
      };
  platform_task_runner.post_task_callback = [](FlutterTask task,
      uint64_t target_time_nanos, void *user_data) -> void {
    static_cast<flutter::TaskRunner*>(user_data)->PostFlutterTask(task,
        target_time_nanos);
  };

  FlutterCustomTaskRunners custom_task_runners = { };
  custom_task_runners.struct_size = sizeof(FlutterCustomTaskRunners);
  custom_task_runners.platform_task_runner = &platform_task_runner;

#ifdef USE_GLES3
  FlutterRendererConfig renderer_config = { };
  renderer_config.type = kOpenGL;
  renderer_config.open_gl.struct_size = sizeof(FlutterOpenGLRendererConfig);
  renderer_config.open_gl.gl_proc_resolver = sunxiegl_proc_resolver;
  renderer_config.open_gl.make_current = sunxiegl_make_current;
  renderer_config.open_gl.make_resource_current = sunxiegl_make_resource_current;
  renderer_config.open_gl.clear_current = sunxiegl_clear_current;
  renderer_config.open_gl.present = sunxiegl_present;
  renderer_config.open_gl.fbo_callback = sunxiegl_fbo_callback;
#else
  FlutterRendererConfig renderer_config = { };
  renderer_config.type = kSoftware;
  renderer_config.software.struct_size = sizeof(FlutterSoftwareRendererConfig);
  renderer_config.software.surface_present_callback = sunxifb_present;
#endif /* USE_GLES3 */

  // FlutterProjectArgs is expecting a full argv, so when processing it for
  // flags the first item is treated as the executable and ignored. Add a dummy
  // value so that all provided arguments are used.
  std::vector<const char*> engine_argv = { "placeholder" };
  //engine_argv.insert(engine_argv.end(), "--verbose-logging");

  FlutterProjectArgs project_args = { };
  project_args.struct_size = sizeof(FlutterProjectArgs);
  project_args.assets_path = assets_path.c_str();
  project_args.icu_data_path = icu_data_path.c_str();
  project_args.aot_data = data;
  project_args.command_line_argc = static_cast<int>(engine_argv.size());
  project_args.command_line_argv = &engine_argv[0];
  project_args.custom_task_runners = &custom_task_runners;
#if 0
  project_args.vsync_callback = [](void *userdata, intptr_t baton) {
    embedder_api_.OnVsync(reinterpret_cast<FlutterEngine>(engine), baton,
        embedder_api_.GetCurrentTime(),
        embedder_api_.GetCurrentTime() + 16.6 * 1e6);
  };

  project_args.platform_message_callback = [](
      const FlutterPlatformMessage *engine_message, void *user_data) -> void {

  };
#endif
  project_args.log_message_callback = [](const char *tag, const char *message,
      void *user_data) {
    std::string str_tag(tag);
    if (str_tag.size() > 0) {
      std::cout << str_tag << ": ";
    }
    std::cout << message << std::endl;
  };

  result = embedder_api_.Run(FLUTTER_ENGINE_VERSION, &renderer_config,
      &project_args, nullptr, &engine_);
  assert(result == kSuccess && engine_ != nullptr);

  FlutterWindowMetricsEvent metrics_event = { };
  metrics_event.struct_size = sizeof(metrics_event);
  metrics_event.width = width;
  metrics_event.height = height;
  metrics_event.pixel_ratio = 1.0;
  result = embedder_api_.SendWindowMetricsEvent(engine_, &metrics_event);
  assert(result == kSuccess);

  auto languages = flutter::GetPreferredLanguageInfo();
  auto flutter_locales = flutter::ConvertToFlutterLocale(languages);

  // Convert the locale list to the locale pointer list that must be provided.
  std::vector<const FlutterLocale*> flutter_locale_list;
  flutter_locale_list.reserve(flutter_locales.size());
  std::transform(flutter_locales.begin(), flutter_locales.end(),
      std::back_inserter(flutter_locale_list),
      [](const auto &arg) -> const auto* {
        return &arg;
      });
  result = embedder_api_.UpdateLocales(engine_, flutter_locale_list.data(),
      flutter_locale_list.size());
  if (result != kSuccess) {
    DEBUG_MSG("Failed to set up Flutter locales.");
  }

  double point_x = 0, point_y = 0, old_point_x = 0, old_point_y = 0, point_tmp = 0;
  int button = 0;
  bool report = false;
  FlutterPointerPhase point_phase;

  while (1) {
    evdev_read(&point_x, &point_y, &button, &report);
    if (report) {
      if (point_phase != FlutterPointerPhase::kMove || button == 1)
        point_phase = (FlutterPointerPhase) button;

      if (swap_touch_xy) {
        point_tmp = point_x;
        point_x = point_y;
        point_y = point_tmp;
      }

      if (flip_touch_x)
        point_x = width - point_x;

      if (flip_touch_y)
        point_y = height - point_y;

      FlutterPointerEvent pointer_event = { };
      pointer_event.struct_size = sizeof(pointer_event);
      pointer_event.phase = point_phase;
      pointer_event.timestamp =
          std::chrono::duration_cast < std::chrono::microseconds
              > (std::chrono::high_resolution_clock::now().time_since_epoch()).count();
      pointer_event.x = point_x;
      pointer_event.y = point_y;
      pointer_event.device = 0;
      pointer_event.signal_kind = kFlutterPointerSignalKindNone;
      pointer_event.scroll_delta_x = 0;
      pointer_event.scroll_delta_y = 0;
      pointer_event.device_kind = kFlutterPointerDeviceKindTouch;
      pointer_event.buttons = 0;

      switch (point_phase) {
      case FlutterPointerPhase::kDown:
        old_point_x = point_x;
        old_point_y = point_y;
        result = embedder_api_.SendPointerEvent(engine_, &pointer_event, 1);

        if (touch_print)
          DEBUG_MSG(
              "point_x=" << point_x << " point_y=" << point_y << " point_phase=" << point_phase);

        point_phase = FlutterPointerPhase::kMove;
        break;
      case FlutterPointerPhase::kUp:
        // Report the same point if the coordinates of down and up are not much different
        // Because the coordinates are inconsistent, flutter will recognize it as a move event
        if (evdev_debounce(old_point_x, old_point_y, point_x, point_y)) {
          pointer_event.x = old_point_x;
          pointer_event.y = old_point_y;
        }

        result = embedder_api_.SendPointerEvent(engine_, &pointer_event, 1);

        if (touch_print)
          DEBUG_MSG(
              "point_x=" << point_x << " point_y=" << point_y << " point_phase=" << point_phase);

        old_point_x = 0;
        old_point_y = 0;
        break;
      case FlutterPointerPhase::kMove:
        // The move event will not be reported if it is the same point as kDown
        if (!evdev_debounce(old_point_x, old_point_y, point_x, point_y)) {
          result = embedder_api_.SendPointerEvent(engine_, &pointer_event, 1);

          if (touch_print)
            DEBUG_MSG(
                "point_x=" << point_x << " point_y=" << point_y << " point_phase=" << point_phase);
        }
        break;
      default:
        point_phase = FlutterPointerPhase::kCancel;
        result = embedder_api_.SendPointerEvent(engine_, &pointer_event, 1);

        if (touch_print)
          DEBUG_MSG(
              "point_x=" << point_x << " point_y=" << point_y << " point_phase=" << point_phase);
        break;
      }
    }

    task_runner_->ProcessTasks();

    std::this_thread::sleep_for(std::chrono::microseconds(16600));
  }

  return EXIT_SUCCESS;
}
