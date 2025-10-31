import 'dart:convert';
import 'dart:core';
import 'dart:async';
import 'dart:io';

import 'package:flutter_dotenv/flutter_dotenv.dart';
import 'package:flutter_phone_direct_caller/flutter_phone_direct_caller.dart';

import 'package:litealert/main.dart';
import 'package:litealert/services/contact_storage_service.dart';

Future<void> _makingPhoneCall() async {
  final ContactStorageService _storageService = ContactStorageService();
  final List<Map<String, String>> contacts = await _storageService.loadContacts();

  for (int i = 0; i < contacts.length; i++) {
    print("MAKING A PHONE CALL! >>> ${contacts[i]['phone']}");
    await FlutterPhoneDirectCaller.callNumber(contacts[i]['phone']!);
  }
}

Future<void> startClient() async {
  String host = dotenv.env['SERVER_HOST']!;
  int port = dotenv.getInt('SERVER_PORT');
  String passphrase = dotenv.env['SERVER_SECRET']!;

  Map<String, dynamic> valueMap = {
    "Yes": 0.00,
    "No": 0.00,
    "Stop": 0.00,
    "Tulong": 0.00,
    "Background_Noise": 0.00,
  };


  try {
    print('Connecting to $host:$port ...');
    final socket = await Socket.connect(host, port, timeout: const Duration(seconds: 5));
    print('Connected to ${socket.remoteAddress.address}:${socket.remotePort}');

    // Listen for server responses
    socket.listen((data) {
      final datastr = utf8.decode(data);
      final responses = utf8.decode(data).split("\n");
      responses.forEach((response) {
        if (response.contains("WakeAlert-ESP32 LOGIN")) {
          homePageStateKey.currentState?.setConnected(true);
          print("Connected to ESP32 successfully!");
        } else if (response.contains(":")) {
          try {
            List<String> pair = response.split(":");
            valueMap[pair[0]] = double.parse(pair[1]);

            print(valueMap);
          } catch (e) {
            print('Ignoring parse error: $e');
          }
        } else if (response.length > 1) {
          print(response); // print without adding extra newline
        }
      });

      if ((valueMap['Background_Noise'] < 0.6 && (
          valueMap['Help'] > 0.7 || valueMap['Tulong'] > 0.7)) || datastr.contains("£")) {
          _makingPhoneCall();
      }
    }, onDone: () {
      print('\nConnection closed by server.');
      socket.destroy();
    }, onError: (err) {
      print('Socket error: $err');
      socket.destroy();
    });

    // Wait for server prompt
    await Future.delayed(const Duration(milliseconds: 200));

    // Send passphrase
    socket.write('$passphrase\n');

  } catch (e) {
    print('Connection failed: $e');
  }
}
