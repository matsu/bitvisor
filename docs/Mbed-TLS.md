# Guide to Create a TLS Connection on BitVisor
This guide describes how to create a TLS connection using BitVisor as a
 client/server with an external Linux server/client.

**Prerequisites**
- Learn more about TLS at Transport Layer Security on wiki
[[ <sup>1</sup>] ](#references).
- Make sure OpenSSL is installed on your system. See OpenSSL Source
[[ <sup>2</sup>] ](#references).

**Table of Contents**
- [Create The Needed Keys And Certification](#create-the-needed-keys-and-certification)
- [Generate Root Certificate Authority Certificates](#generate-root-certificate-authority-certificates)
- [Generate Server Certificates](#generate-server-certificates)
- [BitVisor As The Server](#bitvisor-as-the-server)
- [BitVisor As The Client](#bitvisor-as-the-client)
- [Sending Big Files to TLS Server/Client](#sending-big-files-to-tls-serverclient)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->
---
# Create The Needed Keys And Certification
Assume you currently have two configuration files, RootCA.cnf and server.cnf,
 with the following contents:
- **RootCA.cnf** which is used to set up a Root Certificate Authority (RootCA).
The RootCA is a trusted entity that signs other certificates, establishing a
 foundation of trust.
```
distinguished_name      = req_distinguished_name
x509_extensions         = v3_ca
prompt                  = no

[ req_distinguished_name ]
CN                      = RootCA

[ v3_ca ]
basicConstraints        = critical, CA:TRUE
subjectKeyIdentifier    = hash
authorityKeyIdentifier  = keyid:always, issuer:always
keyUsage                = critical, cRLSign, digitalSignature, keyCertSign
```
- **server.cnf** defines the properties for a server's SSL/TLS certificate
 which sets parameters like the server's distinguished name, key usage, any
 alternative names (for multi-domain certificates), and other extensions
 that define the capabilities and constraints of the certificate.
This certificate is used for secure communications, proving the server's
 identity and enabling encryption.
```
[req]
default_bits            = 2048
distinguished_name      = req_distinguished_name
x509_extensions         = v3_server
prompt                  = no

[ req_distinguished_name ]
C                       = JP
ST                      = TOKYO
L                       = TOKYO
O                       = ABCCOMPANY
CN                      = *

[ v3_server ]
basicConstraints        = critical, CA:TRUE
subjectKeyIdentifier    = hash
authorityKeyIdentifier  = keyid:always, issuer:always
keyUsage                = critical, nonRepudiation, digitalSignature, keyEncipherment, keyAgreement
extendedKeyUsage        = critical, serverAuth
```
# Generate Root Certificate Authority Certificates

**1.Generate a private key for the RootCA:**
This step creates a private key using RSA encryption. This key will be used
 to sign the RootCA certificate.

`openssl genpkey -algorithm RSA -out RootCA.key`

**2.Using the RootCA private key and the RootCA.cnf configuration,
 generate the RootCA certificate:**

The X.509 certificate for the RootCA is created based on the configuration in
 RootCA.cnf. It's self-signed, which means it's trusted as a root certificate,
 and it'll be valid for 3650 days.

`openssl req -new -x509 -days 3650 -key RootCA.key -out RootCA.pem -config
RootCA.cnf`

# Generate Server Certificates
**1.Generate a private key for the server:**

Similar to the RootCA, we first need to generate a private key for the server.
 This key will be used to encrypt and decrypt the server's SSL/TLS
 communications.

`openssl genpkey -algorithm RSA -out server.key`

**2.Using the server private key and the server.cnf configuration,
 generate a Certificate Signing Request (CSR) for the server:**

A CSR is a request for a certificate authority (in our case, the RootCA) to
 sign a public key, creating a certificate. The CSR includes details about the
 entity and the public key.

`openssl req -new -key server.key -out server.csr -config server.cnf`

**3.Using the RootCA private key and certificate,
 sign the server's CSR to generate the server certificate:**

Here, we're asking the RootCA to validate and sign the server's CSR.
 This creates a chain of trust: any client that trusts the RootCA will also
 trust the server's certificate. The resulting certificate (server.pem) is
 valid for 365 days.

_(We assume that the server and RootCA are the same system.
 If they are not, please copy the file server.csr to the RootCA system.)_

`openssl x509 -req -days 365 -in server.csr -CA RootCA.pem -CAkey RootCA.key
-CAcreateserial -out server.pem -extensions v3_server`

---
- **RootCA.key**: This is the private key for the RootCA. It's of paramount
 importance to keep this file secure, as anyone with access can sign
 new certificates, potentially creating security risks.

- **RootCA.pem**: This is the public certificate of the RootCA. This
 certificate should be distributed to any clients or entities that need to
 trust certificates signed by this RootCA.

- **server.key**: This is the server's private key. It should be kept secure
 on the server and is used during SSL/TLS handshakes.

- **server.csr**: This is a temporary file used to request the signing of the
 server's public key. It can be kept for records or deleted.

- **server.pem**: This is the server's certificate, signed by the RootCA.
 It will be presented to clients during SSL/TLS handshakes.

This process allows you to set up a secure, encrypted channel between
 clients and your server, based on trust established by your RootCA.

# BitVisor As The Server

Now that we have all the necessary files ready, we will use BitVisor as the
 server-side and an external Linux machine as the client-side to establish a
 connection using the TLS protocol.

## BitVisor configuration

1. For the purposes of this explanation, let's first consider the case where
 BitVisor is used as the Server. First, take the files that were obtained
 earlier and place them into the **defconfig** before compiling BitVisor.
 These are server.key, server.pem, and RootCA.pem. Assign them in sequence
 to .srv_key, .srv_cert, and .ca_cert within the .tls section as follows :

#Please note the placement of each comma

```
    .tls = {
      .srv_cert =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIC6TCCAdECFDiiW/aGv3Nm+qFpVKxyECyQeH/OMA0GCSqGSIb3DQEBCwUAMBEx\n"
...
        "F7HKk0oI0ZjNOOUjPgWnqgwyYVDP2WyCr5g2cMs=\n"
        "-----END CERTIFICATE-----\n",

      .srv_key =
        "-----BEGIN PRIVATE KEY-----\n"
        "MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBA\n"
...
        "6JN9j0Vy/3SCNjleJ3rkVBQ+SmNNi8iyTjl94d+51elwsosY0lMouDJixUN3yvzs\n"
        "1PABsGsU4X//Us+1DL7/0J4=\n"
        "-----END PRIVATE KEY-----\n",

      .ca_cert =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDQjCCAiqgAwIBAgIUbdMMHizhHnz+psFMmF6Vs4h7wdMwDQYJKoZIhvcNAQEL\n"
...
        "IngZtsfFXq+U8z6sMxaOSJg2/XEHvA==\n"
        "-----END CERTIFICATE-----\n",
    },
```

2. Assuming you're running BitVisor on Qemu,
 try adding '`hostfwd=tcp::11123-:23`' as a parameter in the Qemu command
 line to set up port forwarding. Assuming your BitVisor binary is located at
 ~/x86_test, your bios binary is OVMF.fd, here's an example command :

`./qemu-system-x86_64 -cpu host -enable-kvm -bios ./OVMF.fd -drive
file=fat:rw:/tmp/x86_test/,format=raw -nic user,model=e1000e,
hostfwd=tcp::11123-:23,hostfwd=tcp::10007-:10007, -M q35 -m 4096`

3. To enable telnet-dbgsh, you need to enable this function
 in your **defconfig** file.
```
	.vmm = {
		...
		.telnet_dbgsh = 1,
	...
	},
```
4. To connect to the BitVisor telnet application from another terminal on the
 client machine, use the following command:
`telnet localhost -- -11123`
 This
 command establishes a telnet connection to the local machine (localhost) on
 port 11123. The -- -11123 option tells telnet to connect to port 11123 on the
 local machine, which is being forwarded to port 23 on the
 BitVisor virtual machine.
Once you have successfully connected to the BitVisor telnet application using
 the command , you should see the "**>**" prompt indicating that you are now
 interacting with the BitVisor command line interface.
```
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
>

```
Under the BitVisor telnet prompt, you can enter the command `echoctl_tls` to
 access the Echo Control with TLS functionality. Once you have entered
 echoctl_tls, you can then enter `help` to see the usage and available
 options for the Echo Control feature.
```
> echoctl_tls
echoctl_tls> help
usage:
  client connect <ipaddr> <port> [netif]  Connect to echo server.
  client send                             Send a message to client.
  server start <port> [netif]             Start echo server.

```

5. To initiate the TLS server, execute the following command:
`server start <port>`
This time, we are using 10007 as the port number. Then you will see this
 message, indicating that the server has started working:
```
Starting TLS server (Port: 10007)...
Done.
```
## Client configuration

At present, in BitVisor's configuration, the client side does not require
 mutual authentication, and the use of a digital certificate is also optional.
 The following provides instructions on how to use OpenSSL to connect to
 BitVisor as a server using TLS 1.2.

1. To use OpenSSL as a TLS client, you'll typically start by invoking the
 openssl s_client command in your terminal.
You can use the OpenSSL s_client command to simulate a TLS 1.2 client
 connecting to a remote server. Here is an example command:

`openssl s_client -connect <hostname>:<port> -tls1_2`

2. If a client wants to connect to the BitVisor server using its own public
 certificate RootCA.pem, use the following command:

`openssl s_client -connect <hostname>:<port> -tls1_2 -CAfile RootCA.pem `

If the server's certificate is correct, you will receive the following message:
```
SSL handshake has read 1489 bytes and written 386 bytes
Verification: OK
```
If the client possesses an incorrect public certificate RootCA.pem or the
 server's expected public certificate of does not match, you will receive the
 following message. However you may see the connection is still established:
```
SSL handshake has read 1489 bytes and written 386 bytes
Verification error: certificate signature failure
```
The client and server engage in a TLS handshake process to negotiate encryption
 parameters and establish a secure connection.
Once the TLS handshake is successful, data can be securely exchanged between
 the client and the remote server.
Any relevant information, including server certificates, handshake details,
 and connection status, will be displayed in the terminal.
```
...
Start Time: 1694400959
Timeout : 7200 (sec)
Verify return code: 21 (unable to verify the first certificate)
Extended master secret: yes
---
```
3. At this point, BitVisor acts as an echo server. When you enter some text,
 BitVisor will return the exact same text unchanged.
# BitVisor As The Client
For the purposes of this explanation, let's consider the case where BitVisor
 is used as the Client this time.

1. Let's start with the server side. We'll activate the functionality of the TLS
 server using a simple OpenSSL command

```openssl s_server -key server.key -cert server.pem -tls1_2 -accept <port>```

Make sure your server.key and server.pem files are in the current directory or
 specify the full path to each file.
When you see the following string, your TLS server should be up and running:

```
Using default temp DH parameters
ACCEPT.
```
2. Next, we'll configure BitVisor. First, take the files you obtained earlier
 and place them into the defconfig configuration file. Compile BitVisor as
 we did previously. However, this time, we only require the RootCA.pem.
 Assign it to .ca_cert in the .tls section as follows:
```
      .tls = {
        .ca_cert =
          "-----BEGIN CERTIFICATE-----\n"
          "MIIDQjCCAiqgAwIBAgIUbdMMHizhHnz+psFMmF6Vs4h7wdMwDQYJKoZIhvcNAQEL\n"
...
          "IngZtsfFXq+U8z6sMxaOSJg2/XEHvA==\n"
          "-----END CERTIFICATE-----\n",
      },
```
3. Then, compile and run BitVisor. To activate the telnet functionality within
 BitVisor, closely follow the steps outlined in sections of defconfig in
[BitVisor Configuration](#bitvisor-configuration)

4. Once you're presented with the echoctl_tls> prompt, enter the command:

```echoctl_tls> client connect <server ip> <port>```

If the port number is set to 10007, you should expect the following response:
```
Connecting to TLS server at 192.168.40.50:10007 (c0a82832)
```
5. Upon seeing the echoctl_tls> prompt again, you can confidently input the next
 command :

```echoctl_tls> client send```

Subsequently, the server should start receiving messages from BitVisor, with
 the message reading:
```
Hello, BitVisor with TLS!
```
6. On the server end, feel free to type any string of your choice and hit Enter.
 With the help of OpenSSL, this string will be sent back to BitVisor.
 You'll be able to observe this string within BitVisor's log.
```
Sep 11 15:20:33 warg bitvisor: Received.
Sep 11 15:20:33 warg bitvisor: Test 123 Message from server.
```
# Sending Big Files to TLS Server/Client
The best known configuration in lwipopts.h located in the ip/include/lwip
 directory is as follows:
```
TCP_MSS = 1460
TCP_WND = 16 * TCP_MSS
```
# References
---
[^](#guide-to-create-a-tls-connection-on-bitvisor) [1.TLS on
 wiki](https://en.wikipedia.org/wiki/Transport_Layer_Security) :
 A protocol that provides secure communication over a computer network.

[^](#guide-to-create-a-tls-connection-on-bitvisor) [2.OpenSSL Source
](https://www.openssl.org/source/) : An open source cryptography library.
