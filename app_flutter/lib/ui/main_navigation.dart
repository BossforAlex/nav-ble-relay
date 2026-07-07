/// 主容器：底部导航栏切换三个页面
///
/// 用户需求：
///   - 增加底栏用于切换导航转发、发现设备和设置页面
///   - 删除顶栏下方重复的"导航BLE转发"标题
///   - 暴露 switchToTab() 让首页 FAB 启动服务后能自动跳到"发现设备"页
library;

import 'package:flutter/material.dart';

import 'devices_screen.dart';
import 'home_screen.dart';
import 'settings_screen.dart';

class MainNavigation extends StatefulWidget {
  const MainNavigation({super.key});

  @override
  State<MainNavigation> createState() => MainNavigationState();
}

class MainNavigationState extends State<MainNavigation> {
  int _currentIndex = 0;

  final List<Widget> _screens = const [
    HomeScreen(),
    DevicesScreen(),
    SettingsScreen(),
  ];

  /// 外部（HomeScreen 的 FAB）调用此方法跳转到指定 Tab
  /// index: 0=导航转发 1=发现设备 2=设置
  void switchToTab(int index) {
    if (!mounted) return;
    if (index < 0 || index >= _screens.length) return;
    setState(() => _currentIndex = index);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: IndexedStack(
        index: _currentIndex,
        children: _screens,
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _currentIndex,
        onDestinationSelected: (index) {
          setState(() => _currentIndex = index);
        },
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.navigation_outlined),
            selectedIcon: Icon(Icons.navigation),
            label: '导航转发',
          ),
          NavigationDestination(
            icon: Icon(Icons.bluetooth_searching),
            selectedIcon: Icon(Icons.bluetooth_connected),
            label: '发现设备',
          ),
          NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            selectedIcon: Icon(Icons.settings),
            label: '设置',
          ),
        ],
      ),
    );
  }
}
