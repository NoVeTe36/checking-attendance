import 'dart:convert';
import 'dart:math';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'ecdh_aes.dart'; // đã được bạn cung cấp
import 'ecdsa.dart';   // nếu cần ký hoặc verify, dùng thêm

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'ESP32 WiFi Sender',
      home: const HomePage(),
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});
  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final String id = "ABC123"; // ID cố định
  String status = "Waiting...";

  @override
  void initState() {
    super.initState();
    sendCertToESP32();
  }

  Future<void> sendCertToESP32() async {
    try {
      // 1. Generate 12-byte c_nonce
      final random = Random.secure();
      final cNonce = List<int>.generate(12, (_) => random.nextInt(256));
      final cNonceBase64 = base64Encode(Uint8List.fromList(cNonce));

      // 2. Generate ECDH key pair
      final ecdh = await EcdhKeyGenerator.create(); // từ file ecdh_aes.dart
      final publicKeyBytes = ecdh.publicKeyBytes;
      final publicKeyBase64 = base64Encode(publicKeyBytes);

      // 3. Prepare cert object
      final cert = {
        "id": id,
        "c_nonce": cNonceBase64,
        "pub_key": publicKeyBase64,
      };

      // 4. Gửi đến ESP32
      final response = await http.post(
        Uri.parse("http://192.168.4.1/init"), // chỉnh IP nếu cần
        headers: {"Content-Type": "application/json"},
        body: jsonEncode(cert),
      );

      setState(() {
        status = "ESP32 response: ${response.statusCode} - ${response.body}";
      });
    } catch (e) {
      setState(() {
        status = "Error: $e";
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Cert Sender")),
      body: Center(child: Text(status, style: const TextStyle(fontSize: 18))),
    );
  }
}
