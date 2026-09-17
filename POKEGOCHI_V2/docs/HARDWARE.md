# Hardware do Pokegochi V2

## Conjunto utilizado

| Item | Especificação |
| --- | --- |
| Placa | ESP32-2432S028R, também conhecida como CYD |
| Processador | ESP32-WROOM-32 dual-core, 4 MB flash |
| Tela | TFT 2,8", 240x320, controlador ILI9341 |
| Touch | Resistivo, controlador XPT2046 |
| Armazenamento | microSD em FAT32; protótipo testado com cartão de 256 MB |
| Comunicação | Wi-Fi e Bluetooth/BLE integrados |
| Áudio | Não utilizado |
| Controles | Exclusivamente touchscreen |
| Alimentação de bancada | Micro-USB 5 V |
| Bateria | Li-ion/LiPo protegida 1S, 3,7 V nominal, cerca de 3000 mAh |

O firmware foi construído especificamente para a pinagem dessa placa. Uma
ESP32 DevKit com display separado não é intercambiável sem alterar o firmware.

## Componentes da alimentação final

- 1 célula Li-ion/LiPo protegida 1S, 3,7 V; o protótipo usa aproximadamente
  3020 mAh / 11,7 Wh.
- 1 módulo carregador USB-C com power-path/load sharing. MCP73871 é a opção
  preferida.
- 1 conversor boost MT3608 ou equivalente, ajustado para 5,00 V.
- 1 capacitor eletrolítico low-ESR de 470 µF na saída de 5 V.
- 2 resistores de 100 kΩ, preferencialmente 1%.
- 1 capacitor cerâmico de 100 nF para o ADC da bateria.
- fios flexíveis, tubo termo-retrátil e conector adequado à célula.

Um módulo IP5306 pode substituir carregador e boost em um protótipo compacto,
mas alguns modelos interrompem momentaneamente os 5 V ao conectar o USB. O
MCP73871 com power-path oferece comportamento mais previsível enquanto o jogo
permanece ligado.

## Pinagem usada pelo firmware

| Função | GPIO/pino |
| --- | --- |
| Backlight | GPIO21 |
| SD CS / SCK / MISO / MOSI | GPIO5 / 18 / 19 / 23 |
| Touch CS / IRQ | GPIO33 / 36 |
| Touch SCK / MISO / MOSI | GPIO25 / 39 / 32 |
| LED RGB R / G / B | GPIO4 / 16 / 17, ativo em nível baixo |
| Leitura da bateria | GPIO35 no conector P3, sempre através do divisor |
| Alimentação | 5 V regulados em P1/VIN e GND comum |

## microSD

O pacote V2 ocupa aproximadamente 16 MB. Um cartão de 256 MB já é suficiente;
recomendamos 1 a 4 GB de boa procedência pela confiabilidade, não pela
capacidade. O formato deve ser FAT32 com uma única partição. exFAT e NTFS não
são aceitos pelo firmware atual.
