# Comunicaciones Seguras

## 1. Protocolo TLS

TLS es un protocolo que cifra y autentica datos en tránsito. Importa porque garantiza confidencialidad, integridad y autenticación del servidor. Un certificado es un documento digital firmado por una autoridad que vincula una clave pública con un dominio.

## 2. Riesgos sin TLS

Exposición a espionaje, manipulación de datos, suplantación de servidor y robo de credenciales.

## 3. Certificate Authority (CA)

Entidad que valida identidades y firma certificados para que los clientes confíen en la clave pública presentada por un servidor.

## 4. Cadena de certificados

Secuencia desde el certificado del servidor hasta la CA raíz. La raíz tiene vigencia larga. Las intermedias suelen durar algunos años. El certificado del servidor dura pocos meses o pocos años según la política de la CA.

## 5. Keystore y certificate bundle

Un keystore es un contenedor seguro que almacena claves y certificados. Un bundle es un archivo que agrupa varios certificados, por ejemplo los intermedios y la raíz necesaria para la validación.

## 6. Autenticación mutua en TLS

El servidor valida al cliente y el cliente valida al servidor. Ambos presentan certificados.

## 7. Validación de certificados en ESP32

Se habilita cargando la CA o el bundle en el firmware y usando APIs seguras de WiFiClientSecure o mbedTLS para verificar la firma y la cadena.

## 8. Varios dominios con distintas CAs

Opciones: cargar todas las CAs necesarias en un bundle único, usar verificación insegura solo para pruebas, o implementar descarga dinámica de CA desde almacenamiento externo.

## 9. Obtención del certificado de un dominio

Se puede usar herramientas como `openssl s_client` o descargarlo desde el navegador. También se puede usar ACME para dominios propios.

## 10. Llave pública y privada

La pública se distribuye y sirve para verificar firmas o cifrar datos. La privada permanece protegida y permite firmar o descifrar.

## 11. Consecuencias al expirar certificados

La conexión fallará en la verificación y el ESP32 no establecerá canal seguro hasta actualizar certificados o firmware.

## 12. Fundamento matemático y efecto cuántico

La criptografía moderna usa teoría de números y problemas difíciles como factorización y logaritmo discreto. La computación cuántica puede romper estos esquemas con algoritmos como Shor, lo que impulsa el desarrollo de criptografía poscuántica.

---

