# Solução de problemas

## A porta COM não aparece

- Troque o cabo por um cabo de dados.
- Instale o driver CH340/CH341.
- Feche o monitor serial antes da gravação.
- Tente outra porta USB sem hub.

## `SD FAIL` no diagnóstico

- Confirme FAT32, uma única partição e o cartão totalmente inserido.
- Execute novamente `instalar_sd.ps1`; ele compara o SHA-256 do arquivo copiado.
- Prefira cartão de boa procedência. Capacidade não substitui qualidade.
- Ejete o cartão pelo Windows antes de removê-lo.

## Tela branca ou cores invertidas

Use exatamente a imagem para ESP32-2432S028R. Ela contém a configuração ILI9341
BGR e a inversão exigida pelo painel testado.

## Touch deslocado

Cada XPT2046 possui tolerâncias. A calibração fica em
`include/config/BoardConfig.h`; compile novamente se a sua placa exigir valores
diferentes.

## Indicador de bateria vazio

- Meça o ponto central do divisor.
- Confirme que o resistor superior vai ao BAT+ cru, não aos 5 V do boost.
- Confirme GPIO35/P3 e GND comum.
- Com 4,2 V na célula, o GPIO deve medir perto de 2,1 V.

## A placa reinicia durante animações ou Bluetooth

- Confira se os 5 V permanecem estáveis sob carga.
- Use capacitor de 470 µF na saída do boost.
- Teste com Micro-USB e sem a alimentação externa para separar falha de energia
  de falha do cartão.

## Save recovery

O jogo mantém duas cópias redundantes do save. Não formate nem apague a flash
ao ver a tela de recovery. Conecte o serial e preserve a placa para diagnóstico.
