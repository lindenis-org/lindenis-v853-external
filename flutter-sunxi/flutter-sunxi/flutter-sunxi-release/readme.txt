#######################################################################################
flutter sdk
Flutter 2.5.1 ? channel stable ? https://github.com/flutter/flutter.git
Framework ? revision ffb2ecea52 (4 days ago) ? 2021-09-17 15:26:33 -0400
Engine ? revision b3af521a05
Tools ? Dart 2.14.2

flutter engine
commit id:b3af521a050e6ef076778bcaf16e27b2521df8f8

flutter_fbdev flutter_eglfs
#######################################################################################



#######################################################################################
三个同级目录

flutter					flutter sdk目录，可以访问https://flutter.dev/docs/get-started/install/linux下载，执行git checkout 2.5.1切换到指定版本
flutter_sunxi_release	可以在sunxi平台跑的sdk
sample					应用目录
#######################################################################################



#######################################################################################
complex_layout.tar.gz							预编译的应用，解压后可以推送到/mnt/UDISK/
gallery.tar.gz									预编译的应用，解压后可以推送到/mnt/UDISK/
flutter/bin/cache/artifacts/material_fonts		flutter默认字体文件，可以推送到/usr/share/fonts/	如果有自定义字体，可以不用推送
libflutter_engine.so							flutter engine动态库，可以推送到/mnt/UDISK/

flutter_fbde和flutter_eglfs						预编译的加载flutter app应用，选择一个即可
flutter_fbdev									纯cpu渲染，可以推送到/mnt/UDISK/
flutter_eglfs									gpu渲染，可以推送到/mnt/UDISK/

adb push fonts /usr/share/fonts/
adb push complex_layout /mnt/UDISK/
adb push arm64/flutter_fbdev /mnt/UDISK/
adb push arm64/flutter_eglfs /mnt/UDISK/
adb push arm64/libflutter_engine.so /mnt/UDISK/

运行应用
export LANG="en_US.UTF-8"
cd /mnt/UDISK/
LD_LIBRARY_PATH=./ ./flutter_fbdev complex_layout/

#######################################################################################



#######################################################################################
编译aot app示例

创建app
./flutter/bin/flutter create sample
cd sample

创建编译文件夹
mkdir -p .dart_tool/flutter_build/flutter-embedded-linux
mkdir -p build/linux-embedded-arm64/release/bundle/lib/
mkdir -p build/linux-embedded-arm64/release/bundle/data/

生成资源文件
../flutter/bin/flutter build bundle --asset-dir=build/linux-embedded-arm64/release/bundle/data/flutter_assets

cp ../flutter/bin/cache/artifacts/engine/linux-x64/icudtl.dat build/linux-embedded-arm64/release/bundle/data/

生成快照，最后一句的sample换成app的名称
../flutter/bin/dart \
  --verbose \
  --disable-dart-dev ../flutter/bin/cache/dart-sdk/bin/snapshots/frontend_server.dart.snapshot \
  --sdk-root ../flutter/bin/cache/artifacts/engine/common/flutter_patched_sdk \
  --target=flutter \
  --no-print-incremental-dependencies \
  -Ddart.vm.profile=false \
  -Ddart.vm.product=true \
  --aot \
  --tfa \
  --packages .dart_tool/package_config.json \
  --output-dill .dart_tool/flutter_build/flutter-embedded-linux/app.dill \
  --depfile .dart_tool/flutter_build/flutter-embedded-linux/kernel_snapshot.d \
  package:sample/main.dart

../flutter_sunxi_sdk/arm64/gen_snapshot \
  --deterministic \
  --snapshot_kind=app-aot-elf \
  --elf=.dart_tool/flutter_build/flutter-embedded-linux/libapp.so \
  --strip \
  .dart_tool/flutter_build/flutter-embedded-linux/app.dill

复制编译好的libapp.so到指定路径
cp .dart_tool/flutter_build/flutter-embedded-linux/libapp.so build/linux-embedded-arm64/release/bundle/lib/

推送build/linux-embedded-arm64/release/bundle的bundle到/mnt/UDISK即可，里面有些文件可以删除，目录结构类似complex_layout
#######################################################################################

