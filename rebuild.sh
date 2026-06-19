# 构建脚本

# 强制重新生成 QRC 文件
find resources/ -type f -exec touch {} +

# 构建
cd /tmp/netdiag-build/build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF -DBUILD_SIMULATOR=ON \
    -B . -S "/home/radxa/thinclient_drives/C:/Users/HUJ9/OneDrive - Otis Global Tenant/PowerShell/NetDiagnostic-QT"
ninja net_diagnostic net_diagnostic_sim

# 复制
cp net_diagnostic "/home/radxa/thinclient_drives/C:/Users/HUJ9/OneDrive - Otis Global Tenant/PowerShell/NetDiagnostic-QT/dist/net_diagnostic-Linux-arm64"
cp net_diagnostic_sim "/home/radxa/thinclient_drives/C:/Users/HUJ9/OneDrive - Otis Global Tenant/PowerShell/NetDiagnostic-QT/dist/net_diagnostic_sim-Linux-arm64"

echo "构建完成！"
echo "运行: ./dist/net_diagnostic-Linux-arm64"
echo "模拟器: ./dist/net_diagnostic_sim-Linux-arm64"