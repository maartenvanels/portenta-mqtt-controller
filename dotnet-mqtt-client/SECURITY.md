# Security Best Practices - Portenta MQTT Client

## Credential Management

De .NET MQTT client ondersteunt meerdere methoden voor het beheren van credentials, van minst naar meest secure:

### Methode 1: appsettings.json (Development - NIET AANBEVOLEN voor productie)

**Gebruik:** Development en testing op lokale machine
**Security Level:** ⚠️ Laag - credentials zijn plain-text in bestand

```json
{
  "MqttSettings": {
    "BrokerAddress": "192.168.1.100",
    "BrokerPort": 1883,
    "Username": "your-username",
    "Password": "your-secure-password-here"
  }
}
```

**Risico's:**
- Credentials staan in plain-text
- Kan per ongeluk in git terecht komen
- Leesbaar voor iedereen met toegang tot het bestand

**Mitigation:**
- Gebruik `.gitignore` om `appsettings.Local.json` uit te sluiten
- Gebruik alleen voor development/testing
- Beperk file permissions (Windows: alleen lezen voor huidige user)

---

### Methode 2: appsettings.Local.json (Development - BETER)

**Gebruik:** Lokale development met persoonlijke credentials
**Security Level:** 🟡 Matig - credentials gescheiden van code

Maak een `appsettings.Local.json` die NIET in git komt:

```json
{
  "MqttSettings": {
    "Username": "mijn-username",
    "Password": "mijn-secret-password"
  }
}
```

**Voordelen:**
- Gescheiden van gedeelde configuratie
- Niet in version control (via `.gitignore`)
- Overschrijft appsettings.json waarden

**Setup:**
1. Kopieer `appsettings.Local.json.template` naar `appsettings.Local.json`
2. Vul je credentials in
3. Bestand wordt automatisch geladen als het bestaat

---

### Methode 3: Environment Variables (Production - AANBEVOLEN)

**Gebruik:** Production deployments, CI/CD, containers
**Security Level:** 🟢 Goed - credentials niet in bestanden

De client ondersteunt environment variables:

```bash
# Windows PowerShell
$env:MqttSettings__BrokerAddress = "192.168.18.74"
$env:MqttSettings__Username = "homeassistant"
$env:MqttSettings__Password = "my-secure-password"
dotnet run

# Windows CMD
set MqttSettings__BrokerAddress=192.168.18.74
set MqttSettings__Username=homeassistant
set MqttSettings__Password=my-secure-password
dotnet run

# Linux/macOS
export MqttSettings__BrokerAddress="192.168.18.74"
export MqttSettings__Username="homeassistant"
export MqttSettings__Password="my-secure-password"
dotnet run
```

**Voordelen:**
- Geen credentials in bestanden
- Standaard voor cloud deployments
- Makkelijk te beheren per environment (dev/staging/prod)

**Docker Compose voorbeeld:**
```yaml
services:
  mqtt-client:
    image: portenta-mqtt-client
    environment:
      - MqttSettings__BrokerAddress=192.168.18.74
      - MqttSettings__Username=homeassistant
      - MqttSettings__Password=${MQTT_PASSWORD}
```

---

### Methode 4: Azure Key Vault / Secret Manager (Enterprise - MEEST SECURE)

**Gebruik:** Enterprise production environments
**Security Level:** 🔒 Zeer hoog - centralized secret management

Voor production environments, gebruik een secret manager:

**Azure Key Vault voorbeeld:**
```csharp
// Install: Microsoft.Extensions.Configuration.AzureKeyVault
var keyVaultEndpoint = new Uri(Environment.GetEnvironmentVariable("VaultUri")!);
configuration = new ConfigurationBuilder()
    .AddAzureKeyVault(keyVaultEndpoint, new DefaultAzureCredential())
    .Build();
```

**AWS Secrets Manager:**
```csharp
// Install: AWSSDK.SecretsManager
// Similar pattern met AWS SDK
```

**HashiCorp Vault:**
```csharp
// Install: VaultSharp
// Similar pattern met Vault client
```

---

## Configuration Priority

De client laadt configuratie in deze volgorde (laatste wint):

1. `appsettings.json` (basis configuratie)
2. `appsettings.Local.json` (lokale overrides)
3. Environment Variables (runtime overrides)
4. Command-line arguments (hoogste prioriteit)

**Voorbeeld:**
```bash
# Broker uit environment variable, credentials uit appsettings.Local.json
export MqttSettings__BrokerAddress="10.0.0.5"
dotnet run

# Alles override via command-line
dotnet run 192.168.1.100 1883
```

---

## Security Checklist

### Development
- [ ] Gebruik `appsettings.Local.json` voor persoonlijke credentials
- [ ] Voeg `appsettings.Local.json` toe aan `.gitignore`
- [ ] Commit NOOIT passwords in git history
- [ ] Gebruik strong passwords (min. 16 karakters)
- [ ] Enable MQTT authentication op broker

### Production
- [ ] Gebruik environment variables of secret manager
- [ ] Rotate credentials regelmatig
- [ ] Gebruik TLS/SSL voor MQTT verbinding (poort 8883)
- [ ] Implement rate limiting op MQTT broker
- [ ] Monitor failed authentication attempts
- [ ] Use principle of least privilege (minimal MQTT ACLs)
- [ ] Log security events (zonder passwords te loggen!)

### Network Security
- [ ] Firewall rules voor MQTT broker (alleen noodzakelijke IPs)
- [ ] Gebruik VPN voor remote access
- [ ] Network segmentation (IoT devices op apart netwerk)
- [ ] Disable anonymous MQTT access

---

## TLS/SSL Support (TODO)

Voor secure MQTT verbindingen, update de client om TLS te ondersteunen:

```csharp
var options = new MqttClientOptionsBuilder()
    .WithTcpServer(brokerAddress, 8883)  // TLS port
    .WithTls(new MqttClientOptionsBuilderTlsParameters
    {
        UseTls = true,
        SslProtocol = System.Security.Authentication.SslProtocols.Tls12
    })
    .WithCredentials(username, password)
    .Build();
```

**Certificate Validation:**
```csharp
.WithTls(o =>
{
    o.UseTls = true;
    o.CertificateValidationHandler = context =>
    {
        // Custom certificate validation
        return true;  // or implement proper validation
    };
})
```

---

## Credential Rotation

Voor automated credential rotation:

```csharp
// Herlaad configuratie periodiek
var timer = new Timer(_ =>
{
    LoadConfiguration();
    // Reconnect MQTT client met nieuwe credentials
}, null, TimeSpan.Zero, TimeSpan.FromHours(1));
```

---

## Voorbeeld .gitignore

Zorg dat deze regels in je `.gitignore` staan:

```gitignore
# Sensitive configuration files
appsettings.Local.json
appsettings.Development.json
appsettings.Production.json

# Credentials and keys
*.key
*.pem
*.pfx
credentials.json
secrets.json
.env
.env.*

# User-specific files
*.user
*.suo
```

---

## Security Incident Response

Als credentials zijn gecompromitteerd:

1. **Immediate:**
   - Verander MQTT broker password
   - Revoke/regenerate credentials
   - Check MQTT broker logs voor ongeautoriseerde toegang

2. **Short-term:**
   - Review git history voor gecommitte secrets
   - Gebruik `git-secrets` of `truffleHog` om te scannen
   - Update alle affected systemen

3. **Long-term:**
   - Implement secret scanning in CI/CD
   - Setup alerts voor failed auth attempts
   - Regular security audits

---

## Aanbevolen Tools

- **Secret Scanning:** [git-secrets](https://github.com/awslabs/git-secrets), [truffleHog](https://github.com/trufflesecurity/trufflehog)
- **Password Manager:** 1Password, Bitwarden, LastPass voor team credentials
- **Secret Management:** Azure Key Vault, AWS Secrets Manager, HashiCorp Vault
- **MQTT Security:** MQTT ACLs, Mosquitto auth plugin

---

## Contact

Voor security issues, neem contact op via de project maintainer.
**NIET** rapporteer security issues in public issue tracker!
