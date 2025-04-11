import 'package:flutter/material.dart';
import 'package:flutter_app/player_page.dart';
import 'package:flutter_app/recorder_page.dart';
// import 'package:flutter_app/recorder_page.dart';
import 'package:flutter/services.dart';

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  HomePageState createState() => HomePageState();
}

class HomePageState extends State<HomePage> {
  int _selectedIndex = 0;

  // Using a getter instead of static final to create new instances when needed
  List<Widget> get _pages => [
        PlayerPage(),
        RecorderPage(),
      ];

  void _onItemTapped(int index) {
    HapticFeedback.heavyImpact();
    setState(() {
      _selectedIndex = index;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: _pages[_selectedIndex],
      bottomNavigationBar: BottomNavigationBar(
        items: const <BottomNavigationBarItem>[
          BottomNavigationBarItem(
            icon: Icon(Icons.play_arrow),
            label: 'Player',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.mic),
            label: 'Recorder',
          )
        ],
        currentIndex: _selectedIndex,
        selectedItemColor: Theme.of(context).primaryColor,
        onTap: _onItemTapped,
      ),
    );
  }
}
