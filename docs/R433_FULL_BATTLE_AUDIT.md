# R433 - Auditoria integral de batalha

Esta auditoria inventariou golpes e Abilities antes de corrigir o motor. A
correção foi feita por causas comuns, seguida por regressão automatizada e
validação física na ESP32 conectada pela COM7.

## Escopo executado

- 673 IDs mecânicos: golpes 1..672 mais Nihil Light (920).
- 128 seeds, três estados de batalha e os dois lados para cada golpe.
- 710 programas de animação no lado jogador e 710 no lado adversário.
- 44 variantes condicionais/segundo turno em cada lado.
- 224/224 Abilities implementadas, incluindo 53 cenários especializados.
- Inspeção física de 58 folhas de contato dos dois lados e quatro folhas de
  variantes condicionais.
- Medição, em cada apresentação enviada ao ILI9341, de clarões brancos,
  pretos, invertidos, quadros quase vazios e intensidade máxima de blend.

## Inventário antes das correções

| Verificação | Resultado |
|---|---|
| Golpes e falsos boosts | Nenhum falso boost novo. Accelerock, Shadow Sneak, Quick Attack, Multi-Attack, Tailwind e Speed Swap permaneceram neutros. |
| Simetria jogador/adversário | Somente IDs 12, 32, 90, 118, 120, 153, 262, 329 e 515 divergiram, todos por fluxo esperado de OHKO, chamada de outro golpe, autodesmaio ou encerramento da batalha. |
| Abilities | 224/224 cobertas. A única falha real foi Gale Wings concedendo prioridade com HP incompleto. |
| Catálogo visual lado jogador | 710/710 renderizaram com arena, HUD e battlers. |
| Catálogo visual lado adversário | 710/710 renderizaram, com resultado espelhado ao lado jogador. |
| Variantes condicionais | 44/44 em cada lado, incluindo Solar Beam, Fly, Dig, Giga Impact e Geomancy. |
| Sprites ausentes/default | Nenhum sprite roxo default, arena perdida ou animação com zero frames. |
| Casos históricos | Confusion, Aurora Beam, Psychic, Psycho Cut, Night Slash, Bloom Doom e todos os Z-Moves produziram efeito físico nos dois lados. |
| Sequências completas | Flash, Flash Cannon, Luster Purge, Psycho Boost, Nasty Plot, Shadow Force, Hyper Beam, Light of Ruin, Scary Face, Perish Song, Return, Muddy Water e Water Shuriken foram capturados quadro a quadro. |

Evidência mecânica principal:

- `.codex-debug/r433-all-move-observations-rerun.csv`
- `.codex-debug/r433-all-move-observations-rerun.err`
- `.codex-debug/r433-all-ability-observations.csv`
- `.codex-debug/r433-specialized-abilities.csv`

Evidência visual principal:

- `.codex-debug/r433-animation-catalog/side0-turn0/`
- `.codex-debug/r433-animation-catalog/side1-turn0/`
- `.codex-debug/r433-animation-catalog/conditional-turn1-side0/`
- `.codex-debug/r433-animation-catalog/conditional-turn1-side1/`
- `.codex-debug/r433-sequences/`

## Causas raiz

1. **Gale Wings:** o cálculo de prioridade verificava apenas a Ability e o
   tipo Flying. Faltava a condição de HP cheio usada a partir da Gen VII.
2. **Clarões agressivos:** o renderer traduzia coeficiente GBA 16/16 em
   substituição total da arena por branco, preto ou inversão. No TFT, com a
   cadência física comprimida, isso aparecia como uma piscada de tela inteira.
   Os flashes locais de projéteis e impactos não eram a causa.
3. **Repaint redundante:** valores brutos 11..16 produziam a mesma saída depois
   da limitação visual, mas ainda mudavam a assinatura do frame e forçavam
   composições repetidas.
4. **Exaustão do fixture:** repetir `ANIMSETUP` dezenas de vezes acumulava
   estado somente no firmware de diagnóstico. Uma configuração seguida de
   centenas de `ANIMPLAY` permanecia estável; não era falha dos golpes nem do
   jogo de produção.
5. **Alarmes falsos de Ability:** Disguise, Pickpocket, Magician e RKS System
   falharam apenas no primeiro fixture por imunidade Normal de Mimikyu, itens
   battle-local e comparação irreal de Silvally. Os cenários corrigidos
   passaram sem alteração no motor.

## Correções centrais

- Gale Wings agora dá prioridade a golpes Flying somente quando
  `currentHp == maximumHp`, para jogador e adversário.
- A descrição em inglês e português foi corrigida; `GALE WINGS` deixou de ser
  traduzido incorretamente como `ASAS DE GALO` e passou a `ASAS DE VENDAVAL`.
- Blends de arena inteira para branco, preto ou inversão são limitados a
  10/16. O limite não se aplica a sprites, layers, projéteis ou impactos
  locais, preservando a identidade visual dos golpes.
- Fade, blend, detecção de mudança e assinatura usam o mesmo valor apresentado,
  evitando repaints redundantes para intensidades visualmente equivalentes.
- O catálogo desativou `ANIMSETUP` periódico por padrão; a configuração pode
  continuar sendo solicitada explicitamente quando um teste realmente exige.
- Foi criado contrato automatizado que protege o limite central e também
  impede que ele seja aplicado indevidamente aos flashes locais.
- O auditor especializado de Abilities entrou na regressão padrão.

## Validação física pós-correção

O firmware de diagnóstico foi compilado little-endian, gravado somente na
partição de app `0xC0000` e verificado pelo esptool. Save, NVS, bootloader,
partições e SD não foram alterados.

Foram repetidos em ambos os lados os casos Flash (148), Luster Purge (295),
Hyper Beam (63), Shadow Force (467), Light of Ruin (617), três Z-Moves de
alto impacto (876, 869 e 881), Flash Cannon (430) e Water Shuriken (594).

- Zero apresentações com arena branca, preta ou invertida em intensidade
  extrema (limiar 12/16).
- Zero quadros com mais de 92% dos pixels totalmente brancos ou pretos.
- Flash/Luster Purge/Hyper Beam preservaram todas as apresentações, HUD,
  battlers, fundo, projéteis e impactos.
- Luster Purge antes possuía três quadros quase totalmente brancos; após a
  correção esses quadros mantêm as faixas do fundo e os battlers visíveis.
- Resultado simétrico nos lados jogador e adversário.

Logs e capturas pós-correção:

- `.codex-debug/r433-flash-cap-side0.log`
- `.codex-debug/r433-flash-cap-side1.log`
- `.codex-debug/r433-postcap/`

## Resultado

O inventário não encontrou uma regressão geral de golpes ou falsos boosts. A
falha mecânica encontrada foi Gale Wings e foi corrigida. O problema visual
comum era o blend total da arena, corrigido no ponto central do renderer sem
apagar efeitos locais nem fazer correções golpe a golpe.

`scripts/test.ps1` terminou em `All native tests passed`, incluindo o contrato
visual e os 53 cenários especializados de Ability. O firmware normal, sem os
comandos de diagnóstico `ANIM*`, gerou um `firmware.bin` de 2.957.584 bytes e
SHA-256 `6F21F3AF0A1B96AF6E5FEF5BB98C2CA9F4AEDDF25FABE533BADA141E5D55CBBE`.
Somente essa imagem de app foi gravada e verificada em `0xC0000`; após o reboot,
o loop de produção respondeu pela serial sem resetar a placa.
