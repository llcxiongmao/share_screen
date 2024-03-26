this_dir=$(realpath "$(dirname "$0")")
desktop_dir=$(xdg-user-dir DESKTOP)

echo "[Desktop Entry]
Version=1.0
Name=share_screen
Name[zh_CN]=共享屏幕
Terminal=true
X-MultipleArgs=false
Type=Application
StartupNotify=true
Exec=${this_dir}/share_screen
Icon=${this_dir}/1.svg" > "${desktop_dir}/share_screen.desktop"