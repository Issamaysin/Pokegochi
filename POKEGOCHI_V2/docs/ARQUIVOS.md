# Arquivos da release

| Arquivo | Uso |
| --- | --- |
| `firmware/pokegochi-v2.factory.bin` | Primeira instalação completa em `0x0` |
| `firmware/pokegochi-v2.update.bin` | Aplicativo em `0x20000`, preserva partições existentes |
| `sd/pokegochi-sd-v2.zip` | Estrutura pronta do cartão FAT32 |
| `ota/pokegochi-v2.pgota` | Atualização Bluetooth assinada de firmware + SD |
| `app/PokegochiUpdater-v2.apk` | Aplicativo Android BLE |
| `SHA256SUMS.txt` | Integridade dos artefatos |

O ZIP do cartão contém somente os arquivos necessários em execução:

```text
pokegochi/
  pokegochi.pak
  assets/
    pack_version.txt
    animation_catalog.txt
    manifest.json
```

O repositório não contém as ROMs usadas como fonte. Os scripts de geração ficam
na raiz do projeto para manutenção dos assets pelo proprietário das cópias.
