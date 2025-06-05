import 'dart:typed_data';
import 'dart:math';
import 'package:pointycastle/pointycastle.dart';
import 'package:pointycastle/api.dart';
import 'package:pointycastle/ecc/api.dart';
import 'package:pointycastle/ecc/curves/secp256r1.dart';
import 'package:pointycastle/key_generators/api.dart';
import 'package:pointycastle/key_generators/ec_key_generator.dart';
import 'package:pointycastle/asymmetric/api.dart';
import 'package:pointycastle/block/aes.dart';
import 'package:pointycastle/block/modes/gcm.dart';
import 'package:pointycastle/digests/sha256.dart';
import 'package:pointycastle/macs/hmac.dart';
import 'package:pointycastle/key_derivators/hkdf.dart';
import 'package:crypto/crypto.dart';

class ECDHCrypto {
  static final _secureRandom = SecureRandom('Fortuna')
    ..seed(KeyParameter(
        Uint8List.fromList(List.generate(32, (i) => Random.secure().nextInt(256)))));

  /// Generate ECDH key pair using secp256r1 (P-256) curve
  static AsymmetricKeyPair<ECPublicKey, ECPrivateKey> genKey() {
    final keyGen = ECKeyGenerator();
    final domainParams = ECDomainParameters('secp256r1');
    final keyGenParams = ECKeyGeneratorParameters(domainParams);
    final params = ParametersWithRandom(keyGenParams, _secureRandom);
    keyGen.init(params);
    
    final keyPair = keyGen.generateKeyPair();
    return AsymmetricKeyPair<ECPublicKey, ECPrivateKey>(
      keyPair.publicKey as ECPublicKey,
      keyPair.privateKey as ECPrivateKey,
    );
  }

  /// Convert public key to uncompressed point bytes (X9.62 format)
  /// Compatible with Python's X962 UncompressedPoint encoding
  static Uint8List getPublicBytes(ECPublicKey publicKey) {
    final point = publicKey.Q!;
    
    // Get field size in bytes (32 for secp256r1)
    final fieldSize = 32;
    
    // Convert to uncompressed point format: 0x04 || x || y
    final xBytes = _bigIntToBytes(point.x!.toBigInteger()!, fieldSize);
    final yBytes = _bigIntToBytes(point.y!.toBigInteger()!, fieldSize);
    
    final result = Uint8List(1 + fieldSize * 2);
    result[0] = 0x04; // Uncompressed point indicator
    result.setRange(1, 1 + fieldSize, xBytes);
    result.setRange(1 + fieldSize, 1 + fieldSize * 2, yBytes);
    
    return result;
  }


  /// Perform ECDH key exchange
  static Uint8List getSharedSecret(ECPrivateKey thisPrivateKey, Uint8List otherPublicBytes) {
    if (otherPublicBytes[0] != 0x04) {
      throw ArgumentError('Only uncompressed points are supported');
    }
    
    final domainParams = ECDomainParameters('secp256r1');
    final fieldSize = 32; // 32 bytes for secp256r1
    
    if (otherPublicBytes.length != 1 + fieldSize * 2) {
      throw ArgumentError('Invalid public key length');
    }
    
    final xBytes = otherPublicBytes.sublist(1, 1 + fieldSize);
    final yBytes = otherPublicBytes.sublist(1 + fieldSize, 1 + fieldSize * 2);
    
    final x = _bytesToBigInt(xBytes);
    final y = _bytesToBigInt(yBytes);
    
    final point = domainParams.curve.createPoint(x, y);
    
    final otherPublicKey = ECPublicKey(point, domainParams);


    final ecdh = ECDHBasicAgreement();
    ecdh.init(thisPrivateKey);
    
    final sharedSecret = ecdh.calculateAgreement(otherPublicKey);
    
    // Convert to bytes with proper length (32 bytes for secp256r1)
    return _bigIntToBytes(sharedSecret, 32);
  }

  /// Derive shared key using HKDF-SHA256
  /// Compatible with Python's HKDF implementation
  static Uint8List getSharedKey(Uint8List sharedSecret) {
    const info = 'handshake data';
    const length = 32;
    
    // HKDF Extract: PRK = HMAC-SHA256(salt, IKM)
    // When salt is None/null, use zero-filled salt of hash length
    final salt = Uint8List(32); // 32 zero bytes for SHA256
    final hmacExtract = Hmac(sha256, salt);
    final prk = Uint8List.fromList(hmacExtract.convert(sharedSecret).bytes);
    
    // HKDF Expand: OKM = HMAC-SHA256(PRK, info || 0x01)
    final hmacExpand = Hmac(sha256, prk);
    final infoBytes = Uint8List.fromList(info.codeUnits);
    final expandInput = Uint8List(infoBytes.length + 1);
    expandInput.setRange(0, infoBytes.length, infoBytes);
    expandInput[infoBytes.length] = 0x01;
    
    final okm = Uint8List.fromList(hmacExpand.convert(expandInput).bytes);
    return okm.sublist(0, length);
  }

  /// Encrypt using AES-GCM
  /// Returns (nonce, ciphertext, tag) - same as Python implementation
  static (Uint8List, Uint8List, Uint8List) encrypt(Uint8List key, Uint8List plaintext) {
    final aes = AESEngine();
    final cipher = GCMBlockCipher(aes);
    
    // Generate 12-byte nonce (same as Python)
    final nonce = _generateSecureRandom(12);
    
    // Initialize cipher for encryption
    final params = AEADParameters(KeyParameter(key), 128, nonce, Uint8List(0));
    cipher.init(true, params);
    
    // Encrypt - output includes both ciphertext and tag
    final output = Uint8List(plaintext.length + 16); // plaintext + 16 byte tag
    var len = cipher.processBytes(plaintext, 0, plaintext.length, output, 0);
    len += cipher.doFinal(output, len);
    
    // Split ciphertext and tag
    final ciphertext = output.sublist(0, plaintext.length);
    final tag = output.sublist(plaintext.length, len);
    
    return (nonce, ciphertext, tag);
  }

  /// Decrypt using AES-GCM
  static Uint8List decrypt(Uint8List key, Uint8List nonce, Uint8List ciphertext, Uint8List tag) {
    final aes = AESEngine();
    final cipher = GCMBlockCipher(aes);
    
    // Initialize cipher for decryption
    final params = AEADParameters(KeyParameter(key), 128, nonce, Uint8List(0));
    cipher.init(false, params);
    
    // Combine ciphertext and tag for pointycastle GCM
    final input = Uint8List(ciphertext.length + tag.length);
    input.setRange(0, ciphertext.length, ciphertext);
    input.setRange(ciphertext.length, ciphertext.length + tag.length, tag);
    
    // Decrypt
    final output = Uint8List(ciphertext.length);
    var len = cipher.processBytes(input, 0, input.length, output, 0);
    len += cipher.doFinal(output, len);
    
    return output.sublist(0, ciphertext.length);
  }

  // Helper methods
  static Uint8List _bigIntToBytes(BigInt bigInt, int length) {
    final bytes = Uint8List(length);
    var value = bigInt;
    
    for (int i = length - 1; i >= 0; i--) {
      bytes[i] = (value & BigInt.from(0xff)).toInt();
      value = value >> 8;
    }
    
    return bytes;
  }

  static BigInt _bytesToBigInt(Uint8List bytes) {
    BigInt result = BigInt.zero;
    for (int i = 0; i < bytes.length; i++) {
      result = (result << 8) + BigInt.from(bytes[i]);
    }
    return result;
  }

  static Uint8List _generateSecureRandom(int length) {
    final bytes = Uint8List(length);
    for (int i = 0; i < length; i++) {
      bytes[i] = _secureRandom.nextUint8();
    }
    return bytes;
  }
}

