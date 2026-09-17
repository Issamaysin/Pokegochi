# Instalação da V2

## Requisitos no Windows

- Windows 10 ou 11.
- Cabo Micro-USB de dados.
- Driver CH340/CH341 instalado para a porta serial da placa.
- Python 3 com o pacote `esptool`:

  ```powershell
  py -m pip install --upgrade esptool
  ```

- Leitor de cartão microSD.

Execute primeiro `verificar_release.ps1`. Ele compara todos os arquivos com
`SHA256SUMS.txt` e impede que uma cópia incompleta seja gravada.

## 1. Preparar o cartão

Insira o cartão e confira cuidadosamente a letra da unidade. O comando abaixo
copia e verifica o pacote sem formatar:

```powershell
powershell -ExecutionPolicy Bypass -File .\instalar_sd.ps1 -Drive D:
```

Para apagar e formatar como FAT32 antes da cópia:

```powershell
powershell -ExecutionPolicy Bypass -File .\instalar_sd.ps1 -Drive D: -Format
```

O script nunca formata `C:` nem a unidade que contém a release. Sem `-Yes`, a
formatação exige que a letra da unidade seja digitada novamente.

## 2. Gravar uma placa nova

Coloque o microSD na placa e conecte o cabo USB. Descubra a porta no Gerenciador
de Dispositivos e execute:

```powershell
powershell -ExecutionPolicy Bypass -File .\instalar_firmware.ps1 -Mode Factory -Port COM6
```

`Factory` grava a imagem completa no endereço `0x0`, incluindo bootloader,
partições OTA e aplicativo. Use em placa nova ou quando o save anterior puder
ser descartado.

## 3. Atualizar preservando o save

Se a placa já executa uma versão com o layout OTA atual, use:

```powershell
powershell -ExecutionPolicy Bypass -File .\instalar_firmware.ps1 -Mode Update -Port COM6
```

Essa modalidade grava somente o aplicativo em `0x20000`. Para migrar uma beta
antiga com partições diferentes preservando o save, use o script completo do
repositório: `scripts/gravar_firmware.ps1`. Ele lê, converte, regrava e verifica
a partição de save antes de atualizar.

## 4. Primeira inicialização

1. A tela de diagnóstico deve mostrar display, touch, SD e save como saudáveis.
2. Toque em `START`.
3. O Professor Oak apresenta o jogo e a escolha do inicial.
4. A escolha é permanente; o firmware de produção não possui reset dentro do jogo.
5. Confira em Settings se aparece `FW 2.0 RELEASE` e `ASSETS 22`.

## Atualização Bluetooth

Depois da primeira instalação USB, firmware e SD podem ser atualizados juntos
pelo aplicativo Android. Consulte [ATUALIZACAO_BLUETOOTH.md](ATUALIZACAO_BLUETOOTH.md).
