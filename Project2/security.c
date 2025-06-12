#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "consts.h"
#include "io.h"
#include "libsecurity.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

int state_sec = 0;     // Current state for handshake
char* hostname = NULL; // For client: storing inputted hostname
EVP_PKEY* priv_key = NULL;
tlv* client_hello = NULL;
tlv* server_hello = NULL;

uint8_t ts[1000] = {0};
uint16_t ts_len = 0;

bool inc_mac = false;  // For testing only: send incorrect MACs

void init_sec(int initial_state, char* host, bool bad_mac) {
    state_sec = initial_state;
    hostname = host;
    inc_mac = bad_mac;
    init_io();

    if (state_sec == CLIENT_CLIENT_HELLO_SEND) {
        print("INIT SEND CLIENT HELLO");
        client_hello = create_tlv(CLIENT_HELLO);

        // generate client ephemeral keypair
        generate_private_key();
        derive_public_key();

        // create public key tlv
        tlv* pub_key_tlv = create_tlv(PUBLIC_KEY);
        add_val(pub_key_tlv, public_key, pub_key_size);
        add_tlv(client_hello, pub_key_tlv);

        // generate client nonce
        uint8_t nonce[NONCE_SIZE];
        generate_nonce(nonce, NONCE_SIZE);
        tlv* nonce_tlv = create_tlv(NONCE);
        
        // store both for client_hello construction in input_sec
        add_val(nonce_tlv, nonce, NONCE_SIZE);
        add_tlv(client_hello, nonce_tlv);
        
        ts_len = serialize_tlv(ts, client_hello);
    } else if (state_sec == SERVER_CLIENT_HELLO_AWAIT) {
        // no immediate action, wait for client hello in output_sec
    }
}

ssize_t input_sec(uint8_t* buf, size_t max_length) {
    switch (state_sec) {
    case CLIENT_CLIENT_HELLO_SEND: {
        print("SEND CLIENT HELLO");
        if (client_hello == NULL) {
            client_hello = create_tlv(CLIENT_HELLO);

            // generate public/private key pair
            generate_private_key();
            derive_public_key();
            tlv *pk = create_tlv(PUBLIC_KEY);
            add_val(pk, public_key, pub_key_size);
            add_tlv(client_hello, pk);

            // nonce
            uint8_t nonce[NONCE_SIZE];
            generate_nonce(nonce, NONCE_SIZE);
            tlv *nc = create_tlv(NONCE);
            add_val(nc, nonce, NONCE_SIZE);
            add_tlv(client_hello, nc);

            // add to transcript
            ts_len = serialize_tlv(ts, client_hello);
        }

        uint16_t out_len = serialize_tlv(buf, client_hello);

        state_sec = CLIENT_SERVER_HELLO_AWAIT;
        return out_len;
    }
    case SERVER_SERVER_HELLO_SEND: {
        print("SEND SERVER HELLO");
        server_hello = create_tlv(SERVER_HELLO);

        // nonce
        uint8_t nonce[NONCE_SIZE];
        generate_nonce(nonce, NONCE_SIZE);
        tlv *nonce_tlv = create_tlv(NONCE);
        add_val(nonce_tlv, nonce, NONCE_SIZE);
        add_tlv(server_hello, nonce_tlv);

        // access certificate
        load_certificate("server_cert.bin");
        tlv *cert_tlv = deserialize_tlv(certificate, cert_size);
        if (!cert_tlv || cert_tlv->type != CERTIFICATE) exit(1);
        add_tlv(server_hello, cert_tlv);      

        // generate ephemeral public key
        generate_private_key();       
        derive_public_key();           
        tlv *eph_pub_tlv = create_tlv(PUBLIC_KEY);
        add_val(eph_pub_tlv, public_key, pub_key_size);
        add_tlv(server_hello, eph_pub_tlv);

        // load server private key
        EVP_PKEY *eph_priv = get_private_key();     
        load_private_key("server_key.bin");       

        // set up handshake signature
        uint8_t to_sign[2000];
        uint16_t off = 0;
        off += serialize_tlv(to_sign + off, client_hello);
        off += serialize_tlv(to_sign + off, nonce_tlv);
        off += serialize_tlv(to_sign + off, cert_tlv);
        off += serialize_tlv(to_sign + off, eph_pub_tlv);

        uint8_t sig_buf[255];
        size_t  sig_len = sign(sig_buf, to_sign, off);

        // restore ephemeral private key
        set_private_key(eph_priv);

        // add handshake signature to server hello
        tlv *sig_tlv = create_tlv(HANDSHAKE_SIGNATURE);
        add_val(sig_tlv, sig_buf, sig_len);
        add_tlv(server_hello, sig_tlv);

        // add to transcript
        uint16_t out_len = serialize_tlv(buf, server_hello);

        memcpy(ts + ts_len, buf, out_len);
        ts_len += out_len;

        state_sec = SERVER_FINISHED_AWAIT;
        return out_len;
    } 
    case CLIENT_FINISHED_SEND: {
        print("SEND FINISHED");

        // verify server certificate using CA public key
        load_ca_public_key("ca_public_key.bin");
        tlv* cert_tlv = get_tlv(server_hello, CERTIFICATE);
        tlv* dns_tlv = get_tlv(cert_tlv, DNS_NAME);
        tlv* cert_pubkey_tlv = get_tlv(cert_tlv, PUBLIC_KEY);
        tlv* sig_tlv = get_tlv(cert_tlv, SIGNATURE);
        uint8_t signed_data[1000];
        uint16_t offset = 0;
        offset += serialize_tlv(signed_data + offset, dns_tlv);
        offset += serialize_tlv(signed_data + offset, cert_pubkey_tlv);
        if (verify(sig_tlv->val, sig_tlv->length, signed_data, offset, ec_ca_public_key) != 1) {
            exit(1);
        }

        // DNS name validation
        if (hostname == NULL)
            exit(2);
        size_t host_len = strlen(hostname);
        if (dns_tlv->length != host_len + 1 || memcmp(dns_tlv->val, hostname, host_len) != 0)
            exit(2);

        // verify Server Hello was signed by server
        tlv* eph_pub_key_tlv = get_tlv(server_hello, PUBLIC_KEY);
        if (!eph_pub_key_tlv) 
            exit(6);
        tlv *server_nonce = get_tlv(server_hello, NONCE);         
        tlv *server_cert  = get_tlv(server_hello, CERTIFICATE); 
        tlv *server_eph_key = get_tlv(server_hello, PUBLIC_KEY);
        tlv *handshake_sig = get_tlv(server_hello, HANDSHAKE_SIGNATURE);

        if (!server_nonce || !server_cert || !server_eph_key || !handshake_sig)   
            exit(6); // unexpected message

        // retrieve public key from server's certificate
        tlv  *cert_pubkey = get_tlv(server_cert, PUBLIC_KEY);
        const unsigned char *p = cert_pubkey->val;
        EVP_PKEY *server_pubkey = d2i_PUBKEY(NULL, &p, cert_pubkey->length);
        if (!server_pubkey || EVP_PKEY_base_id(server_pubkey) != EVP_PKEY_EC)
            exit(3); // bad signature                                

        // rebuild the message that the server signed
        uint8_t to_sign[2000];
        uint16_t tlen = 0;
        tlen += serialize_tlv(to_sign + tlen, client_hello);
        tlen += serialize_tlv(to_sign + tlen, server_nonce);
        tlen += serialize_tlv(to_sign + tlen, server_cert);
        tlen += serialize_tlv(to_sign + tlen, server_eph_key);

        // verify signature                                                
        if (verify(handshake_sig->val, handshake_sig->length, to_sign, tlen, server_pubkey) != 1)
        {
            EVP_PKEY_free(server_pubkey);
            exit(3); // signature verification failed                               
        }
        EVP_PKEY_free(server_pubkey);
        

        load_peer_public_key(eph_pub_key_tlv->val, eph_pub_key_tlv->length);

        // use client's private key + server ephemeral pubkey to derive secret
        derive_secret();

        // use HKDF to get ENC and MAC keys
        derive_keys(ts, ts_len);

        // compute transcript HMAC digest of client_hello + server_hello
        uint8_t mac[MAC_SIZE];
        hmac(mac, ts, ts_len);

        // build finished message
        tlv* transcript_tlv = create_tlv(TRANSCRIPT);
        add_val(transcript_tlv, mac, MAC_SIZE);
        tlv* finished_tlv = create_tlv(FINISHED);
        add_tlv(finished_tlv, transcript_tlv);

        ssize_t len = serialize_tlv(buf, finished_tlv);
        state_sec = DATA_STATE;
        return len;
    }
    case DATA_STATE: {
        // read up to 943 bytes from stdin
        uint8_t plaintext[943];
        ssize_t plaintext_len = read(STDIN_FILENO, plaintext, sizeof(plaintext));

        if (plaintext_len <= 0) {
            return 0; //EOF
        }
        
        // encrypt data using ENC key and IV
        uint8_t iv[IV_SIZE];
        uint8_t ciphertext[943 + 16];
        ssize_t cipherLength = encrypt_data(iv, ciphertext, plaintext, plaintext_len);

        if (cipherLength <= 0) {
            fprintf(stderr, "ENCRYPTION FAILED");
            return 0;
        }

        fprintf(stderr, "Encrypted to %zd bytes\n", cipherLength);

        // compute hMAC over IV || ciphertext using MAC key
        if (cipherLength > 1024 || cipherLength <= 0) {
            fprintf(stderr, "Invalid cipherLength: %zd\n", cipherLength);
            return 0;
        }

        uint8_t iv_serial_buf[1024];
        uint8_t ciphertext_serial_buf[1024];
        
        // wrap IV, Ciphertext, MAC into their TLVs
        tlv* iv_tlv = create_tlv(IV);
        add_val(iv_tlv, iv, IV_SIZE);
        tlv* ciphertext_tlv = create_tlv(CIPHERTEXT);
        add_val(ciphertext_tlv, ciphertext, cipherLength);
        ssize_t iv_serial_len = serialize_tlv(iv_serial_buf, iv_tlv);
        ssize_t ciphertext_serial_len = serialize_tlv(ciphertext_serial_buf, ciphertext_tlv);

        uint8_t* mac_input = malloc(iv_serial_len + ciphertext_serial_len);
        memcpy(mac_input, iv_serial_buf, iv_serial_len);
        memcpy(mac_input + iv_serial_len, ciphertext_serial_buf, ciphertext_serial_len);

        uint8_t mac[MAC_SIZE];
        hmac(mac, mac_input, iv_serial_len + ciphertext_serial_len);

        tlv* mac_tlv = create_tlv(MAC);
        add_val(mac_tlv, mac, MAC_SIZE);

        // wrap in DATA TLV and serialize
        tlv* data_tlv = create_tlv(DATA);
        add_tlv(data_tlv, iv_tlv);
        add_tlv(data_tlv, ciphertext_tlv);
        add_tlv(data_tlv, mac_tlv);

        ssize_t len = serialize_tlv(buf, data_tlv);
        print_tlv_bytes(buf, len);
        return len;
    }
    default:
        return 0;
    }
}

void output_sec(uint8_t* buf, size_t length) {
    switch (state_sec) {
    case SERVER_CLIENT_HELLO_AWAIT: {
        print("SERVER CLIENT HELLO AWAIT");
        client_hello = deserialize_tlv(buf, length);
        // validate structure of Client Hello
        if (!get_tlv(client_hello, NONCE) || !get_tlv(client_hello, PUBLIC_KEY)) {
            exit(1);
        }
        // store deserialized value in global variable
        tlv* pubkey_tlv = get_tlv(client_hello, PUBLIC_KEY);
        load_peer_public_key(pubkey_tlv->val, pubkey_tlv->length);
        ts_len = serialize_tlv(ts, client_hello);

        state_sec = SERVER_SERVER_HELLO_SEND;
        break;
    }
    case CLIENT_SERVER_HELLO_AWAIT: {
        print("CLIENT SERVER HELLO AWAIT");

        // extract and store server nonce, cert, ephemeral pubkey, signature
        server_hello = deserialize_tlv(buf, length);
        tlv* nonce_tlv = get_tlv(server_hello, NONCE);
        tlv* cert_tlv = get_tlv(server_hello, CERTIFICATE);
        tlv* pubkey_tlv = get_tlv(server_hello, PUBLIC_KEY);
        tlv* sig_tlv = get_tlv(server_hello, HANDSHAKE_SIGNATURE);
        load_peer_public_key(pubkey_tlv->val, pubkey_tlv->length);
        
        // append to transcript 
        ts_len += serialize_tlv(ts + ts_len, server_hello);

        state_sec = CLIENT_FINISHED_SEND;
        break;
    }
    case SERVER_FINISHED_AWAIT: {
        // deserialize Finished TLV
        tlv* finished_tlv = deserialize_tlv(buf, length);
        if (!finished_tlv)
            exit(6); // unexpected message

        // extract transcript MAC
        tlv* transcript = get_tlv(finished_tlv, TRANSCRIPT);

        // derive ENC amd MAC keys using stored server priv key + client pubkey
        derive_secret();
        derive_keys(ts, ts_len);

        // compute expected transcript HMAC
        uint8_t expected_mac[MAC_SIZE];
        hmac(expected_mac, ts, ts_len);

        // if mismatch, exit
        if (memcmp(expected_mac, transcript->val, MAC_SIZE) != 0) {
            exit(4); // transcript mismatch
        }
        state_sec = DATA_STATE;
        break;
    }
    case DATA_STATE: {
        tlv* data = deserialize_tlv(buf, length);

        // parse nested TLVs: IV, Ciphertext, MAC
        tlv* iv_tlv = get_tlv(data, IV);
        tlv* ciphertext_tlv = get_tlv(data, CIPHERTEXT);
        tlv* mac_tlv = get_tlv(data, MAC);

        // Serialize IV and Ciphertext TLVs like the sender did
        uint8_t iv_serial_buf[1024];
        uint8_t ciphertext_serial_buf[1024];
        ssize_t iv_serial_len = serialize_tlv(iv_serial_buf, iv_tlv);
        ssize_t ciphertext_serial_len = serialize_tlv(ciphertext_serial_buf, ciphertext_tlv);

        uint8_t* mac_input = malloc(iv_serial_len + ciphertext_serial_len);
        memcpy(mac_input, iv_serial_buf, iv_serial_len);
        memcpy(mac_input + iv_serial_len, ciphertext_serial_buf, ciphertext_serial_len);

        // Compute HMAC over identical serialized input
        uint8_t expected_mac[MAC_SIZE];
        hmac(expected_mac, mac_input, iv_serial_len + ciphertext_serial_len);

        //If not invalid exit(5)
        if (memcmp(expected_mac, mac_tlv->val, MAC_SIZE) != 0) {
            fprintf(stderr, "MAC verification failed\n");
            free(mac_input);
            exit(5);
        }

        free(mac_input);

        // decrypt ciphertext using ENC key and IV
        uint8_t plaintext[943];
        size_t plaintext_len = decrypt_cipher(plaintext, ciphertext_tlv->val, ciphertext_tlv->length, iv_tlv->val);

        // write plaintext to stdout
        write(STDOUT_FILENO, plaintext, plaintext_len);

        break;
    }
    default:
        break;
    }
}