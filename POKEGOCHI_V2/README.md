# Pokegochi v2.2

Pacote completo da versão 2.2 para a placa ESP32-2432S028R de 2,8 polegadas.
Esta pasta foi feita para ficar na raiz do repositório e reúne, em um único
lugar, os arquivos de instalação, a imagem do microSD e a documentação do
hardware realmente usado no projeto.

## Instalação rápida

1. Leia [docs/INSTALACAO.md](docs/INSTALACAO.md).
2. Insira um microSD no computador e execute:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\instalar_sd.ps1 -Drive D:
   ```

3. Coloque o cartão na placa, conecte a ESP32 por USB e execute:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\instalar_firmware.ps1 -Mode Factory -Port COM6
   ```

4. Em uma placa que já possui o layout OTA da V1, use `-Mode Update` para
   atualizar somente o aplicativo e preservar o save.

Também é possível preparar os dois componentes em sequência:

```powershell
powershell -ExecutionPolicy Bypass -File .\instalar_tudo.ps1 -Drive D: -Port COM6 -Mode Factory
```

## Conteúdo

- `firmware/`: imagem completa para primeira instalação e imagem de atualização.
- `sd/`: pacote pronto para copiar para um cartão FAT32.
- `ota/`: atualização assinada de firmware + SD para o aplicativo Android.
- `app/`: aplicativo Android para atualização Bluetooth.
- `docs/`: hardware, bateria, soldagem, instalação e diagnóstico.
- `SHA256SUMS.txt`: hashes de todos os arquivos de distribuição.
- `verificar_release.ps1`: valida os arquivos antes da gravação.

## Hardware principal

- ESP32-2432S028R (CYD), display ILI9341 240x320 e touch XPT2046.
- microSD FAT32; o pacote ocupa menos de 20 MB.
- célula Li-ion/LiPo protegida 1S, 3,7 V nominal.
- carregador com power-path e conversor regulado para 5,00 V.
- divisor de tensão 100 kΩ + 100 kΩ no GPIO35 para o indicador de bateria.

O esquema elétrico completo está em
[docs/BATERIA_E_SOLDAGEM.md](docs/BATERIA_E_SOLDAGEM.md). Não ligue uma célula
Li-ion diretamente a um GPIO nem alimente a entrada de 5 V com a tensão crua da
bateria.

## Verificação

Antes de instalar:

```powershell
powershell -ExecutionPolicy Bypass -File .\verificar_release.ps1
```

Os assets do jogo são derivados localmente das cópias de jogo fornecidas pelo
proprietário do projeto. Nenhuma ROM original faz parte do repositório.
