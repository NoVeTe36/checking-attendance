import 'dart:typed_data';
import 'dart:math';
import 'dart:convert';
import 'package:pointycastle/export.dart';
import 'package:asn1lib/asn1lib.dart';

class ECDSA {
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

  /// Encrypt using AES-GCM
  /// Returns (nonce, ciphertext, tag) - same as Python implementation
  static Uint8List sign(ECPrivateKey privateKey, Uint8List message) {
    final signer = Signer("SHA-256/ECDSA")
      ..init(true, ParametersWithRandom(PrivateKeyParameter<ECPrivateKey>(privateKey), _secureRandom));

    final sig = signer.generateSignature(message) as ECSignature;

    // === 4. Encode signature as DER (for Python compatibility) ===
    final seq = ASN1Sequence();
    seq.add(ASN1Integer(sig.r));
    seq.add(ASN1Integer(sig.s));
    final derSig = seq.encodedBytes!;
    return derSig;
  }

  /// Decrypt using AES-GCM
  static bool verify(Uint8List message, Uint8List publicBytes, Uint8List signature) {
    if (publicBytes[0] != 0x04) {
      throw ArgumentError('Only uncompressed points are supported');
    }
    
    final domainParams = ECDomainParameters('secp256r1');
    final fieldSize = 32; // 32 bytes for secp256r1
    
    if (publicBytes.length != 1 + fieldSize * 2) {
      throw ArgumentError('Invalid public key length');
    }
    
    final xBytes = publicBytes.sublist(1, 1 + fieldSize);
    final yBytes = publicBytes.sublist(1 + fieldSize, 1 + fieldSize * 2);
    
    final x = _bytesToBigInt(xBytes);
    final y = _bytesToBigInt(yBytes);
    
    final point = domainParams.curve.createPoint(x, y);
    
    final publicKey = ECPublicKey(point, domainParams);


    final verifier = Signer("SHA-256/ECDSA")
      ..init(false, PublicKeyParameter<ECPublicKey>(publicKey));

    final parser = ASN1Parser(signature);
    final sequence = parser.nextObject() as ASN1Sequence;

    final r = (sequence.elements[0] as ASN1Integer).valueAsBigInteger;
    final s = (sequence.elements[1] as ASN1Integer).valueAsBigInteger;

    final sig =  ECSignature(r, s);

    final isValid = verifier.verifySignature(message, sig);
    return isValid;
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

