from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
import os


def gen_key():
    private_key = ec.generate_private_key(ec.SECP256R1())
    public_key = private_key.public_key()
    return private_key, public_key

def get_public_bytes(public_key) -> bytes:
    public_bytes = public_key.public_bytes(
        encoding=serialization.Encoding.X962,
        format=serialization.PublicFormat.UncompressedPoint
    )
    return public_bytes

def get_shared_secret(this_private_key, other_public_bytes):
    other_public_key = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), other_public_bytes)
    return this_private_key.exchange(ec.ECDH(), other_public_key)

def get_shared_key(shared_secret: bytes) -> bytes:
    return HKDF(
        algorithm=hashes.SHA256(),
        length=32,
        salt=None,
        info=b'handshake data',
    ).derive(shared_secret)

def encrypt(key: bytes, plaintext: bytes) -> tuple[bytes, bytes, bytes]:
    """Ciphertext has the same length of plaintext
    nonce is 12 bytes
    tag is 16 bytes
    """
    aesgcm = AESGCM(key)
    nonce = os.urandom(12)
    ciphertext_tag = aesgcm.encrypt(nonce, plaintext, None)

    tag_size = 16
    ciphertext = ciphertext_tag[:-tag_size]
    tag = ciphertext_tag[-tag_size:]
    return nonce, ciphertext, tag

def decrypt(key: bytes, nonce: bytes, ciphertext: bytes, tag: bytes):
    aesgcm = AESGCM(key)
    ciphertext_tag = ciphertext + tag
    plaintext = aesgcm.decrypt(nonce, ciphertext_tag, None)
    return plaintext


if __name__ == "__main__":
    # Gen key
    private_key, public_key = gen_key()
    

    # Serialize public key to send to ESP32
    public_bytes = get_public_bytes(public_key)
    print("python Public key bytes in hex:", public_bytes.hex().upper())


    # example receive the public bytes
    esp_key_string = input("esp pub: ")
    # replace with real bytes 
    esp32_public_bytes = bytes.fromhex(esp_key_string)  

    # Compute shared secret
    shared_secret = get_shared_secret(private_key, esp32_public_bytes)
    print("Shared secret:", shared_secret.hex().upper())



    # Derive AES key from shared secret with HKDF
    shared_key = get_shared_key(shared_secret)
    print("Derived AES key:", shared_key.hex().upper())

    # Encrypt "hello" with AES-GCM
    nonce, ciphertext, tag = encrypt(shared_key, b"hello") 

    print("AES-GCM Nonce:", nonce.hex().upper())
    print("AES-GCM Ciphertext:", ciphertext.hex().upper())
    print("AES-GCM tag:", tag.hex().upper())


    # Decrypt 
    # received data
    esp_nonce = nonce
    esp_ciphertext = ciphertext
    esp_tag = tag
    decrypted = decrypt(shared_key, esp_nonce, esp_ciphertext, esp_tag)
    print("Decrypted text:", decrypted.decode())


