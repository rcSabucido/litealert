import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:simple_ripple_animation/simple_ripple_animation.dart';
import 'package:liquid_progress_indicator_v2/liquid_progress_indicator.dart';

Future getPermissions()async{
  try{
    await Permission.bluetooth.request();
  } catch(e) {
    print(e.toString());
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key, required this.title});

  // This widget is the home page of your application. It is stateful, meaning
  // that it has a State object (defined below) that contains fields that affect
  // how it looks.

  // This class is the configuration for the state. It holds the values (in this
  // case the title) provided by the parent (in this case the App widget) and
  // used by the build method of the State. Fields in a Widget subclass are
  // always marked "final".

  final String title;

  @override
  State<HomePage> createState() => HomePageState();
}

class HomePageState extends State<HomePage> {
  //int _counter = 0;
  int batteryPercentage = 75;
  bool connected = false;

  @override
  void initState() {
    super.initState();
    getPermissions();
  }

  void setConnected(bool newConnected) {
    setState(() { connected = newConnected; });
  }

  /*
  void _incrementCounter() {
    setState(() {
      // This call to setState tells the Flutter framework that something has
      // changed in this State, which causes it to rerun the build method below
      // so that the display can reflect the updated values. If we changed
      // _counter without calling setState(), then the build method would not be
      // called again, and so nothing would appear to happen.
      _counter++;
    });
  }
  */

  @override
  Widget build(BuildContext context) {
    // This method is rerun every time setState is called, for instance as done
    // by the _incrementCounter method above.
    //
    // The Flutter framework has been optimized to make rerunning build methods
    // fast, so that you can just rebuild anything that needs updating rather
    // than having to individually change instances of widgets.
    return Scaffold(
      appBar: PreferredSize(
        preferredSize: Size(100, 100),
        child: SafeArea(
          child: Container(
            height: 140,
            color: Theme.of(context).colorScheme.inversePrimary,
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Expanded(
                  child: Center(
                    child: Padding(
                      padding: EdgeInsets.all(24.0),
                      child: ListView(
                        shrinkWrap: true,
                        children: [
                          Text("Hello there,"),
                          Text(
                            "John Doe",
                            style: TextStyle(fontWeight: FontWeight.bold),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
                Center(
                  child: SizedBox(
                    width: 128,
                    height: 65,
                    child: FittedBox(child: const Icon(Icons.verified_user)),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
      body: Center(
        // Center is a layout widget. It takes a single child and positions it
        // in the middle of the parent.
        child: Column(
          // Column is also a layout widget. It takes a list of children and
          // arranges them vertically. By default, it sizes itself to fit its
          // children horizontally, and tries to be as tall as its parent.
          //
          // Column has various properties to control how it sizes itself and
          // how it positions its children. Here we use mainAxisAlignment to
          // center the children vertically; the main axis here is the vertical
          // axis because Columns are vertical (the cross axis would be
          // horizontal).
          //
          // TRY THIS: Invoke "debug painting" (choose the "Toggle Debug Paint"
          // action in the IDE, or press "p" in the console), to see the
          // wireframe for each widget.
          mainAxisAlignment: MainAxisAlignment.center,
          children: <Widget>[
            Text(
              connected
                  ? 'Your WakeAlert device has\nbeen connected successfully.'
                  : 'Connect your WakeAlert device via\nthe same Wi-Fi network as your phone.',
              textAlign: TextAlign.center,

              style: Theme.of(context).textTheme.titleLarge,
            ),
            Padding(
              padding: EdgeInsets.only(top: 50, bottom: 40),
              child: connected
                  ? SizedBox(
                      width: 256,
                      height: 256,
                      child: LiquidCircularProgressIndicator(
                        value: math.max(
                          math.min(100.0, batteryPercentage / 100.0),
                          0,
                        ),
                        valueColor: AlwaysStoppedAnimation(
                          Theme.of(context).colorScheme.inversePrimary,
                        ), // Defaults to the current Theme's accentColor.
                        backgroundColor: Colors
                            .white, // Defaults to the current Theme's backgroundColor.
                        borderColor: Theme.of(context).colorScheme.surface,
                        borderWidth: 5.0,
                        direction: Axis
                            .vertical, // The direction the liquid moves (Axis.vertical = bottom to top, Axis.horizontal = left to right). Defaults to Axis.vertical.
                        center: Text(
                          "$batteryPercentage%",
                          style: TextStyle(
                            color: Theme.of(context).colorScheme.tertiary,
                            fontWeight: FontWeight.w800,
                            fontSize: 32,
                          ),
                        ),
                      ),
                    )
                  : RippleAnimation(
                      color: Colors.deepPurple,
                      delay: const Duration(milliseconds: 500),
                      repeat: true,
                      minRadius: 64,
                      maxRadius: 96,
                      ripplesCount: 8,
                      duration: const Duration(milliseconds: 6 * 700),
                      child: CircleAvatar(
                        minRadius: 144,
                        maxRadius: 144,
                        child: SizedBox.expand(
                          child: FittedBox(
                            fit: BoxFit.fill,
                            child: Padding(
                              padding: EdgeInsets.all(10),
                              child: Icon(Icons.wifi_tethering),
                            ),
                          ),
                        ),
                      ),
                    ),
            ),
            Container(
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(24),
                color: Theme.of(context).colorScheme.inversePrimary,
                boxShadow: [
                  BoxShadow(
                    color: Theme.of(context).colorScheme.inversePrimary,
                    spreadRadius: 3,
                  ),
                ],
              ),
              child: Padding(
                padding: EdgeInsets.only(
                  left: 40,
                  right: 40,
                  top: 20,
                  bottom: 20,
                ),
                child: Text(
                  connected ? "Connected" : "Connecting",
                  style: Theme.of(context).textTheme.titleLarge,
                ),
              ),
            ),
            /*
            const Text('You have pushed the button this many times:'),
            Text(
              '$_counter',
              style: Theme.of(context).textTheme.headlineMedium,
            ),
            */
          ],
        ),
      ),
      /*
      floatingActionButton: FloatingActionButton(
        onPressed: _incrementCounter,
        tooltip: 'Increment',
        child: const Icon(Icons.add),
      ), // This trailing comma makes auto-formatting nicer for build methods.
      */
    );
  }
}
