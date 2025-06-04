import 'package:flutter/material.dart';
import 'package:flutter_foreground_task/flutter_foreground_task.dart';
import 'background_task.dart';
import 'dart:io';
import 'package:path_provider/path_provider.dart';
import 'package:http/http.dart' as http;
import 'ecdsa.dart';
import 'ecdh_aes.dart';
import 'dart:typed_data';
import 'package:shared_preferences/shared_preferences.dart';
import 'dart:math';
import 'dart:convert';

String? ecdsa_pub;

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});
  @override
  Widget build(BuildContext context) {
    return const MaterialApp(home: HomePage());
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});
  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final TextEditingController _idController = TextEditingController();
  final TextEditingController _tokenController = TextEditingController();

  @override
  void initState() {
    super.initState();
    // _startForegroundTask();
  }

  // void _startForegroundTask() {
  //   FlutterForegroundTask.init(
  //     androidNotificationOptions: AndroidNotificationOptions(
  //       channelId: 'foreground_channel_id',
  //       channelName: 'ESP32 Background',
  //       channelDescription: 'Sending data periodically...',
  //       channelImportance: NotificationChannelImportance.LOW,
  //       priority: NotificationPriority.LOW,
  //       iconData: NotificationIconData(
  //         resType: ResourceType.mipmap,
  //         resPrefix: ResourcePrefix.ic,
  //         name: 'launcher',
  //       ),
  //     ),
  //     iosNotificationOptions: const IOSNotificationOptions(),
  //     foregroundTaskOptions: const ForegroundTaskOptions(
  //       interval: 5000,
  //       isOnceEvent: false,
  //       autoRunOnBoot: false,
  //     ),
  //   );

  //   FlutterForegroundTask.startService(
  //     notificationTitle: 'ESP32 Uploader',
  //     notificationText: 'Sending cert every 5s...',
  //     callback: startCallback,
  //   );
  // }

  void _sendData() async {
    final id = _idController.text.trim();
    final token = _tokenController.text.trim();
    final ecdsaKeyPair = ECDSA.genKey();
    final ecdsaPubBytes = ECDSA.getPublicBytes(ecdsaKeyPair.publicKey);
    final ecdasPrivate = ecdsaKeyPair.privateKey;
    final keyHex = _bytesToHex(ecdsaPubBytes).toUpperCase();

    ecdsa_pub = keyHex;

    final dir = await getApplicationDocumentsDirectory();
    final file = File('${dir.path}/ecdsa.pem');

    if (await file.exists()) {
      print("File aleardy exists, deleting and creating new one");
      await file.delete();
      final pemContent = ECDSA.ecPrivateKeyToPEM(ecdasPrivate);
      print(pemContent);
      await file.writeAsString(pemContent);
    } else {
      final pemContent = ECDSA.ecPrivateKeyToPEM(ecdasPrivate);
      print(pemContent);
      await file.writeAsString(pemContent);
    }
    if (id.isEmpty || token.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Please fill both ID and Token")),
      );
      return;
    }

    final url = Uri.parse("http://192.168.86.64:5000/sign_cert");

    final response = await http.post(
      url,
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({'id': id, 'token': token, 'pub_key': keyHex}),
    );

    if (response.statusCode == 200) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Send successfully! ECDSA key saved.")),
      );
      final Map<String, dynamic> data = jsonDecode(response.body);

      await saveToFile('ca.pub', data['ca_pub']);
      await saveToFile('signature', data['signature']);
      await saveToLocalStorage('valid_until', data['valid_until']);
      await saveToLocalStorage('id', id);
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Send fail: ${response.statusCode}")),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Background Sender')),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            TextField(
              controller: _idController,
              decoration: const InputDecoration(labelText: 'ID'),
            ),
            TextField(
              controller: _tokenController,
              decoration: const InputDecoration(labelText: 'Token'),
            ),
            const SizedBox(height: 20),
            ElevatedButton(onPressed: _sendData, child: const Text('Sign in')),
            ElevatedButton(
              onPressed: _authenticate,
              child: const Text('Authenticate'),
            ),
            const SizedBox(height: 20),
            const Text('Đang gửi dữ liệu mỗi 5 giây'),
          ],
        ),
      ),
    );
  }

  String _bytesToHex(Uint8List bytes) {
    final buffer = StringBuffer();
    for (var b in bytes) {
      buffer.write(b.toRadixString(16).padLeft(2, '0'));
    }
    return buffer.toString();
  }

  Future<void> _authenticate() async {
    final prefs = await SharedPreferences.getInstance();
    final id = prefs.getString('id');
    final validUntil = prefs.getString('valid_until');

    final signature = await readFile('signature');

    final cNonce = _randomBytes(12);

    final ecdhKeyPair = await ECDHCrypto.genKey();
    final ecdhPubBytes = ECDHCrypto.getPublicBytes(ecdhKeyPair.publicKey);

    final url = Uri.parse("http://192.168.4.1/authenticate");

    final response = await http.post(
      url,
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({id: id, 'valid_until': validUntil, 'pub': ecdsa_pub, 'signature': signature, 'c_nonce': _bytesToHex(Uint8List.fromList(cNonce)), 'session': _bytesToHex(ecdhPubBytes).toUpperCase()}),
    );
  }

  Future<String?> readFile(String filename) async {
    try {
      final dir = await getApplicationDocumentsDirectory();
      final file = File('${dir.path}/$filename');

      if (await file.exists()) {
        final content = await file.readAsString();
        return content;
      } else {
        print('File not found: $filename');
        return null;
      }
    } catch (e) {
      print('Error reading $filename: $e');
      return null;
    }
  }

  Future<void> saveToFile(String filename, String content) async {
    final dir = await getApplicationDocumentsDirectory();
    final file = File('${dir.path}/$filename');
    await file.writeAsString(content);
  }

  Future<void> saveToLocalStorage(String id, String validUntil) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('device_id', id);
    await prefs.setString('valid_until', validUntil);
  }

  
  List<int> _randomBytes(int length) {
    final rnd = Random.secure();
    return List.generate(length, (_) => rnd.nextInt(256));
  }
}

@override
Widget build(BuildContext context) {
  return Scaffold(
    appBar: AppBar(title: const Text('Background Sender')),
    body: const Center(child: Text('Đang gửi dữ liệu mỗi 5 giây')),
  );
}

@pragma('vm:entry-point')
void startCallback() {
  FlutterForegroundTask.setTaskHandler(MyTaskHandler());
}
