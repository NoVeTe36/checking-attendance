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

    print("id: $id");
    print("ecdsaPubBytes: $keyHex");

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

    print(response.body);

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

  static String _bytesToHex(Uint8List bytes) {
    final buffer = StringBuffer();
    for (var b in bytes) {
      buffer.write(b.toRadixString(16).padLeft(2, '0'));
    }
    return buffer.toString();
  }

  static Uint8List _hexToBytes(String hex) {
    final cleanedHex = hex.replaceAll(RegExp(r'\s+'), '');
    final length = cleanedHex.length;
    final result = Uint8List(length ~/ 2);

    for (int i = 0; i < length; i += 2) {
      final byte = cleanedHex.substring(i, i + 2);
      result[i ~/ 2] = int.parse(byte, radix: 16);
    }

    return result;
  }

  Future<void> _authenticate() async {
    final prefs = await SharedPreferences.getInstance();
    final myId = prefs.getString('id');
    final validUntil = prefs.getString('valid_until');

    final signature = await readFile('signature');

    final cNonce = _randomBytes(12);

    final ecdhKeyPair = await ECDHCrypto.genKey();
    final ecdhPubBytes = ECDHCrypto.getPublicBytes(ecdhKeyPair.publicKey);

    final url = Uri.parse("http://192.168.4.1/handshake");

    print(
      "id: $myId, validUntil: $validUntil, ecdhPubBytes: ${_bytesToHex(ecdhPubBytes).toUpperCase()}",
    );
    print("cNonce: ${_bytesToHex(Uint8List.fromList(cNonce))}");
    print("signature: $signature");

    final response = await http.post(
      url,
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'id': myId,
        'valid_until': validUntil,
        'pub': ecdsa_pub,
        'signature': signature,
        'c_nonce': _bytesToHex(Uint8List.fromList(cNonce)),
        'session': _bytesToHex(ecdhPubBytes).toUpperCase(),
      }),
    );

    if (response.statusCode == 200) {
      final Map<String, dynamic> data = jsonDecode(response.body);
      final serverId = data['id'];
      final validUntil = data['valid_until'];
      final pub = data['pub'];
      final session_signature = data['session_signature'];
      final cert_signature = data['cert_signature'];
      final session = data['session'];
      final s_nonce = data['s_nonce'];

      print(data);

      final certBytes = ECDSA.certBytes(
        ascii.encode(serverId),
        _hexToBytes(pub),
        ascii.encode(validUntil),
      );
      if (ECDSA.verify(
        certBytes,
        _hexToBytes((await readFile('ca.pub'))!),
        _hexToBytes(cert_signature),
      )) {
        print("valid server cert");
      } else {
        print("invalid server cert");
        return;
      }

      final sessionBytes = ECDSA.sessionBytes(
        Uint8List.fromList(cNonce),
        ecdhPubBytes,
        _hexToBytes(session),
      );

      if (ECDSA.verify(
        sessionBytes,
        _hexToBytes(pub),
        _hexToBytes(session_signature),
      )) {
        print("valid server session");
      } else {
        print("invalid server session");
        return;
      }

      final authURL = Uri.parse("http://192.168.4.1/authenticate");

      final signaturePrivateKey = ECDSA.pemToECPrivateKey(
        (await readFile('ecdsa.pem'))!,
      );

      final returnSessionBytes = ECDSA.sessionBytes(
        _hexToBytes(s_nonce),
        ecdhPubBytes,
        _hexToBytes(session),
      );

      final returnSessionSig = ECDSA.sign(
        signaturePrivateKey,
        returnSessionBytes,
      );

      print("s_nonce: $s_nonce");
      print("ecdhPubBytes: ${_bytesToHex(ecdhPubBytes).toUpperCase()}");
      print("ecdsa_pub: $ecdsa_pub");
      print("session: ${_bytesToHex(_hexToBytes(session)).toUpperCase()}");
      print("returnSessionSig: ${_bytesToHex(returnSessionSig).toUpperCase()}");

      final response2 = await http.post(
        authURL,
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({
          'id': myId,
          'signature': _bytesToHex(returnSessionSig).toUpperCase(),
          'pub': ecdsa_pub,
          'session': _bytesToHex(ecdhPubBytes).toUpperCase(),
        }),
      );

      print("authentication response: ${response2.body}");
    } else {
      print('Authentication failed:');
      return;
    }
  }

  Future<String?> readFile(String filename) async {
    try {
      final dir = await getApplicationDocumentsDirectory();
      final file = File('${dir.path}/$filename');

      if (await file.exists()) {
        final content = await file.readAsString();
        print("Read from file: ${file.path}");
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
    print("Saving to file: ${file.path}");
    await file.writeAsString(content);
  }

  Future<void> saveToLocalStorage(String id, String value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(id, value);
  }

  List<int> _randomBytes(int length) {
    final rnd = Random.secure();
    print("Generating $length random bytes $rnd");
    return List.generate(length, (_) => rnd.nextInt(256));
  }
}

@override
Widget build(BuildContext context) {
  return Scaffold(
    appBar: AppBar(title: const Text('Check Attendance Application')),
  );
}

@pragma('vm:entry-point')
void startCallback() {
  FlutterForegroundTask.setTaskHandler(MyTaskHandler());
}
