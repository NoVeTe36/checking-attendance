import 'dart:convert';
import 'dart:math';
import 'dart:typed_data';
import 'package:flutter_foreground_task/flutter_foreground_task.dart';
import 'package:http/http.dart' as http;
import 'dart:isolate';
import 'ecdsa.dart';
import 'ecdh_aes.dart';

class MyTaskHandler extends TaskHandler {
  final String id = "A123456";
  final String esp32Url = "http://192.168.4.1/upload";

  @override
  Future<void> onStart(DateTime timestamp, SendPort? sendPort) async {}

  @override
  void onRepeatEvent(DateTime timestamp, SendPort? sendPort) {
    onEvent(timestamp, sendPort);
  }

  @override
  Future<void> onEvent(DateTime timestamp, SendPort? sendPort) async {
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

      final payload = {
        "id": id,
        "ecdsa_public_key": _bytesToHex(ecdsaPubBytes).toUpperCase(),
        "valid": valid,
        "signature": _bytesToHex(signature).toUpperCase(),
        "c_nonce": base64Encode(cNonce),
        "session_ecdh_public_key": _bytesToHex(ecdhPubBytes).toUpperCase(),
        "nonce": base64Encode(nonce),
        "ciphertext": base64Encode(ciphertext),
        "tag": base64Encode(tag),
      };

      await http.post(
        Uri.parse(esp32Url),
        headers: {"Content-Type": "application/json"},
        body: jsonEncode(payload),
      );
    } catch (_) {}
  }

  @override
  Future<void> onDestroy(DateTime timestamp, SendPort? sendPort) async {}

  String _bytesToHex(Uint8List bytes) {
    final buffer = StringBuffer();
    for (var b in bytes) {
      buffer.write(b.toRadixString(16).padLeft(2, '0'));
    }
    return buffer.toString();
  }

  List<int> _randomBytes(int length) {
    final rnd = Random.secure();
    return List.generate(length, (_) => rnd.nextInt(256));
  }
}
