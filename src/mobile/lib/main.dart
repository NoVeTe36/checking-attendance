import 'dart:convert';
import 'dart:math';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'dart:async';

import 'ecdsa.dart';
import 'ecdh_aes.dart';

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
  final String id = "A123456";
  final String esp32Url = "http://192.168.4.1/upload";
  String status = "Not send";
  Timer? _timer;

  @override
  void initState() {
    super.initState();
    sendCertAndKeys();
    _timer = Timer.periodic(const Duration(seconds: 5), (_) {
      sendCertAndKeys();
    });
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Attendance Application")),
      body: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            ElevatedButton(
              onPressed: sendCertAndKeys,
              child: const Text("Send Certificate and Keys"),
            ),
            const SizedBox(height: 20),
            Text(status),
          ],
        ),
      ),
    );
  }

  Future<void> sendCertAndKeys() async {
    setState(() {
      status = "Gen key and signature...";
    });

    try {
      final ecdsaKeyPair = ECDSA.genKey();
      final ecdsaPubBytes = ECDSA.getPublicBytes(ecdsaKeyPair.publicKey);
      final valid = DateTime.now().millisecondsSinceEpoch;
      final idBytes = utf8.encode(id);
      final dataToSign = Uint8List.fromList([...idBytes, ...ecdsaPubBytes]);
      final signature = ECDSA.sign(ecdsaKeyPair.privateKey, dataToSign);
      final cNonce = _randomBytes(12);

      final ecdhKeyPair = await ECDHCrypto.genKey();
      final ecdhPubBytes = ECDHCrypto.getPublicBytes(ecdhKeyPair.publicKey);

      final sharedSecret = ECDHCrypto.getSharedSecret(
        ecdhKeyPair.privateKey,
        ecdsaPubBytes,
      );

      final sharedKey = ECDHCrypto.getSharedKey(sharedSecret);

      final plaintext = utf8.encode("Secure data");
      final (nonce, ciphertext, tag) = ECDHCrypto.encrypt(
        sharedKey,
        Uint8List.fromList(plaintext),
      );

      final Map<String, dynamic> payload = {
        "id": id,
        "ecdsa_public_key": bytesToHex(ecdsaPubBytes).toUpperCase(),
        "valid": valid,
        "signature": bytesToHex(signature).toUpperCase(),
        "c_nonce": base64Encode(cNonce),
        "session_ecdh_public_key": bytesToHex(ecdhPubBytes).toUpperCase(),
        "nonce": base64Encode(nonce),
        "ciphertext": base64Encode(ciphertext),
        "tag": base64Encode(tag),
      };

      setState(() {
        status = "Sending to ESP32...";
      });

      final resp = await http.post(
        Uri.parse(esp32Url),
        headers: {"Content-Type": "application/json"},
        body: jsonEncode(payload),
      );

      if (resp.statusCode == 200) {
        setState(() {
          status = "Send successfully!";
        });
      } else {
        setState(() {
          status = "Error while sending: ${resp.statusCode}";
        });
      }
    } catch (e) {
      setState(() {
        status = "Error: $e";
      });
    }
  }

  String bytesToHex(Uint8List bytes) {
    final buffer = StringBuffer();
    for (var b in bytes) {
      buffer.write(b.toRadixString(16).padLeft(2, '0'));
    }
    return buffer.toString();
  }

  List<int> _randomBytes(int length) {
    final rnd = Random.secure();
    return List<int>.generate(length, (_) => rnd.nextInt(256));
  }
}
