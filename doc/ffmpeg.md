# Windows
- 安装msys2。
- msys2下安装依赖工具（已经安装过了就忽略），运行msys2_shell.cmd（位于msys2安装目录下），在shell终端下运行：
```
pacman -S pkg-config
pacman -S make
pacman -S yasm
pacman -S diffutils
```
- 打开visual studio command line。在此cmd下运行：msys2_shell.cmd -use-full-path。
- 在打开的shell中运行：
```sh
src_dir=/c/my/3rd/ffmpeg-5.1.1
build_dir=/c/my/3rd/build/ffmpeg
install_dir=/c/my/3rd/install/ffmpeg

release_build_dir=${build_dir}/release

mkdir -p ${release_build_dir} && \
cd ${release_build_dir} && \
${src_dir}/configure \
--disable-autodetect \
--target-os=win64 \
--arch=x86_64 \
--toolchain=msvc \
--prefix=${install_dir} \
--disable-programs \
--disable-doc \
--extra-cflags="-MT" \
--extra-cxxflags="-MT" && \
make install
```
PS
- 运行configure很慢，所以耐心等待，不是卡死了。
- configure --help查看全部选项。

# Linux
依赖工具：
```
sudo apt install pkg-config
sudo apt install yasm
sudo apt install diffutils
```
执行sh：
```sh
src_dir=/home/llc/下载/my/3rd/ffmpeg-5.1.1
build_dir=/home/llc/下载/my/3rd/build/ffmpeg
install_dir=/home/llc/下载/my/3rd/install/ffmpeg/

release_build_dir=${build_dir}/release

mkdir -p ${release_build_dir} && \
cd ${release_build_dir} && \
${src_dir}/configure \
--extra-cflags="-static-libgcc -static-libstdc++" \
--extra-cxxflags="-static-libgcc -static-libstdc++" \
--arch=x86_64 \
--prefix=${install_dir} \
--disable-programs \
--disable-doc && \
make install
```

# Mac
依赖工具：
```
brew install pkg-config
brew install automake
brew install yasm
brew install diffutils
```
执行sh：
```sh
src_dir=/Users/dabao/Desktop/my/3rd/ffmpeg-5.1.1
build_dir=/Users/dabao/Desktop/my/3rd/build/ffmpeg
install_dir=/Users/dabao/Desktop/my/3rd/install/ffmpeg/

release_build_dir=${build_dir}/release

mkdir -p ${release_build_dir} && \
cd ${release_build_dir} && \
${src_dir}/configure \
--arch=x86_64 \
--prefix=${install_dir} \
--disable-programs \
--disable-doc && \
make install
```