from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric.utils import (
    encode_dss_signature, decode_dss_signature
)

def gen_signature_key():
    private_key = ec.generate_private_key(ec.SECP256R1())
    public_key = private_key.public_key()
    return private_key, public_key

def load_private_key(private_pem_bytes: bytes):
    private_key = serialization.load_pem_private_key(
        private_pem_bytes,
        password=None,  
        backend=default_backend()
    )
    public_key = private_key.public_key()
    return private_key, public_key

def export_private_key(private_key) -> bytes:
    return private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.TraditionalOpenSSL,
        encryption_algorithm=serialization.NoEncryption()
    )

def get_public_bytes(public_key) -> bytes:
    public_bytes = public_key.public_bytes(
        encoding=serialization.Encoding.X962,
        format=serialization.PublicFormat.UncompressedPoint
    )
    return public_bytes

def cert_bytes(id: str, public_bytes: bytes, valid_until: str) -> bytes:
    """id string 6 chars
    public_bytes: 65 bytes
    valid_until: datetime string "%Y-%m-%d %H:%M:%S"
    """
    return id.encode("ascii") + public_bytes + valid_until.encode("ascii")

def session_bytes(nonce: bytes, c_pub_bytes: bytes, s_pub_bytes: bytes) -> bytes:
    return nonce + c_pub_bytes + s_pub_bytes

def sign(private_key, message: bytes) -> bytes:
    return private_key.sign(
        message,
        ec.ECDSA(hashes.SHA256())
    )
    
def verify(message: bytes, public_bytes: bytes, signature: bytes) -> bool:
    public_key = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), public_bytes)
    try:
        public_key.verify(
            signature,
            message,
            ec.ECDSA(hashes.SHA256())
        )
        return True
    except Exception as e:
        return False


if __name__ == "__main__":
    private_key, public_key = gen_signature_key()
    private_key_pem = export_private_key(private_key)

    public_bytes = get_public_bytes(public_key)
    message = b"tao la tung"
    signature = sign(private_key, message)

    print("private_key:", private_key_pem.decode())
    print("public_bytes:", public_bytes.hex().upper())
    print("signature:", signature.hex().upper());


    print("verify:", verify(message, public_bytes, signature))
        
    esp_public_string = input("esp public:")
    esp_public_bytes = bytes.fromhex(esp_public_string)

    esp_signature_string = input("esp signature:")
    esp_signature_bytes = bytes.fromhex(esp_signature_string)

    print("verify esp signature:", verify(message, esp_public_bytes, esp_signature_bytes))
