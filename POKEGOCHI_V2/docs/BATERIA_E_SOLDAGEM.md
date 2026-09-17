# Bateria, carregamento e indicador

## Avisos importantes

- Use somente célula 1S protegida contra sobrecarga, descarga excessiva e
  sobrecorrente.
- Nunca solde diretamente no corpo de uma célula sem terminais próprios.
- Nunca conecte BAT+ diretamente a um GPIO.
- Ajuste e confira a saída do boost em 5,00 V antes de conectar a ESP32.
- Todos os GNDs precisam estar em comum.
- Não una duas fontes de 5 V independentes. Para gravar por Micro-USB durante
  a bancada, desconecte a alimentação externa de 5 V da placa.

## Caminho de potência

```text
USB-C 5 V
   |
   v
carregador MCP73871 com power-path
   | BAT ---------------- célula protegida 1S 3,7 V
   | SYS
   v
boost MT3608 ajustado para 5,00 V
   |
   +---- capacitor 470 µF ---- GND
   |
   +---- P1/VIN 5 V da ESP32-2432S028R
GND -------------------------- P1/GND da placa
```

A entrada de energia da placa recebe **5 V regulados**. A tensão crua da
célula varia de aproximadamente 3,0 a 4,2 V e não deve alimentar diretamente
essa entrada no projeto final.

## Fio do indicador de bateria

O indicador precisa medir a tensão crua da célula, antes do boost. Medir o
terminal de 5 V do conversor faz o firmware enxergar uma tensão inválida e o
ícone fica vazio.

```text
BAT+ da célula ---- 100 kΩ ----+---- GPIO35 / P3
                               |
                             100 kΩ
                               |
BAT- / GND comum ---------------+---- GND da ESP32

GPIO35 / P3 -------- 100 nF -------- GND
```

### Pontos de solda

1. Solde o resistor superior de 100 kΩ ao **BAT+ real**, não ao `5V OUT`.
2. Una a outra ponta dele ao ponto central do divisor.
3. Do ponto central, solde um fio fino ao GPIO35/P3.
4. Solde o segundo resistor de 100 kΩ entre o ponto central e GND.
5. Solde o capacitor de 100 nF entre o ponto central e GND, próximo da placa.
6. Confirme continuidade de GND entre bateria/BMS, conversor e ESP32.
7. Isole todas as emendas com termo-retrátil.

### Valores esperados

| Tensão da célula | Tensão aproximada no GPIO35 |
| --- | --- |
| 4,20 V | 2,10 V |
| 3,70 V | 1,85 V |
| 3,30 V | 1,65 V |

Se o GPIO receber perto de 2,50 V porque o divisor foi ligado à saída de 5 V,
o firmware calculará aproximadamente 5 V para a célula e rejeitará a leitura.

## Como o firmware mede

- GPIO35 usa ADC1 com atenuação de 11 dB.
- São feitas quatro medições espaçadas por 200 ms.
- O valor mostrado é a média das quatro leituras.
- A tela inicial atualiza o indicador a cada 30 segundos.
- O ícone usa quatro barras, pois a porcentagem de uma Li-ion medida apenas por
  tensão é aproximada, principalmente durante carga ou acesso intenso ao SD.

## Teste antes de fechar o gabinete

1. Teste carregador e célula sem a ESP32.
2. Confirme que a célula não aquece durante a carga.
3. Regule o boost em 5,00 V e faça um teste com carga de pelo menos 500 mA.
4. Verifique a polaridade em P1/VIN e P1/GND duas vezes.
5. Meça BAT+ e o ponto central do divisor com multímetro.
6. Ligue a placa e acompanhe no serial a linha `[BAT]`.
7. Teste tela, SD e Bluetooth com o brilho máximo antes de fechar a case.
