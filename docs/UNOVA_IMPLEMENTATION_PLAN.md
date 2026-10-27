# Plano de implementação de Unova

## Contrato aprovado

- Unova é desbloqueada somente depois da conclusão de Sinnoh.
- O desbloqueio libera retroativamente os learnsets e TMs da geração 5 para
  todas as espécies compatíveis das gerações anteriores.
- A campanha possui dez áreas procedurais. O jogador escolhe até três Pokémon;
  HP, PP e status persistem durante a incursão; a Bag fica bloqueada; existe
  uma única cura por área; perder ou abandonar reinicia apenas a área atual.
- Cada incursão apresenta salas com caminhos, NPCs que fornecem pistas, um
  Gate Trainer que precisa ser encontrado e derrotado e, depois, o Boss da
  área. Vencer o Boss libera permanentemente a área seguinte.
- Capturas são eventos de exploração: grama balançando, nuvem de poeira,
  sombra, água borbulhando e Hidden Grotto. Hidden Grottos podem entregar um
  item ou um Pokémon com Hidden Ability.
- Lendários nunca são encontros garantidos. Eventos próprios usam a mesma
  raridade-base do Wild/Sinnoh; um local especial pode aplicar somente um
  aumento pequeno e continua permitindo novas tentativas.
- Pokémon World Tournament aparece nos checkpoints da campanha.
- A Área 10 termina contra Benga, sem recompensa de Shiny Dratini.
- Champions Tournament conclui Unova e libera o tema Black.
- Royal Unova abre depois da conclusão no mesmo Challenge Hub que contém
  Battle Tower e Boss, com desafio diário de três a sete treinadores.
- A Box e a Pokédex passam a comportar toda a National Dex até #649, com
  migração integral dos saves atuais.
- DNA Splicers, Drives, Secret Sword, Relic Song, Zen Mode e as demais formas
  da geração 5 alteram sprite, mini-ícone, tipo, stats, habilidade e moves
  exatamente onde cada mecânica exige. Os gênios usam o `FORM` comum da Box.

## Arquitetura

1. Congelar os layouts V45 de Box/Pokédex e criar a migração V46.
2. Importar dados canônicos de Pokémon, golpes, evoluções e learnsets da ROM
   local de Pokémon White, usando o histórico de pokeemerald-expansion para
   assets e para as formas introduzidas em White 2.
3. Remapear as habilidades exclusivas das Megas para uma faixa sem colisão
   com as habilidades oficiais da geração 5.
4. Criar `UnovaAdventure` como domínio independente da interface: geração de
   salas, persistência de incursão, pistas, cura, Gate Trainer, Boss,
   fenômenos, Hidden Ability, checkpoints e conclusão.
5. Integrar Unova ao Region Hub, Market, Box, Pokédex, Summary, batalha,
   estatísticas, multiplayer e ao Challenge Hub.
6. Empacotar sprites autênticos das espécies/formas e construir as telas com
   composição retida, preload e cache, sem leituras de SD no meio de frames.
7. Cobrir migração, progressão, raridade, formas, TMs, batalha e navegação com
   testes nativos e contratos estáticos antes da validação no ESP32.

## Critérios de entrega

- Nenhum save V45 ou anterior perde Pokémon, inventário ou progresso.
- Gen 5 não vaza para Market, learnsets ou encontros antes do desbloqueio.
- Nenhuma forma usa placeholder; todos os caminhos de batalha, Box, Home e
  Summary apontam para assets existentes no pack.
- A campanha pode ser concluída sem farming de botão e respeita o estado
  persistente de cada área.
- A suíte nativa completa e a auditoria do pacote SD passam antes de qualquer
  build de firmware. O build e a validação física ficam para o lote solicitado
  pelo usuário e não fazem parte desta entrega em fonte.

## Estado da implementação em fonte

- Concluído: National Dex #001-649, 559 moves, 165 habilidades oficiais de
  Gen I-V, evoluções/learnsets retroativos, 25 TMs de Unova e bloqueio de todo
  o conteúdo Gen 5 até a conclusão de Sinnoh.
- Concluído: save V46 com migração dos formatos anteriores, Box para 720
  indivíduos, Pokédex #649, estatísticas de lendários/Shiny e protocolo
  multiplayer compatível com espécies, formas, moves e Hidden Abilities Gen 5.
- Concluído: dez áreas procedurais, grupo congelado de até três Pokémon,
  persistência de HP/PP/status, Bag desativada, cura única, pistas, Gate
  Trainer, Boss, abandono/derrota por área e prevenção de farming em salas já
  consumidas.
- Concluído: grama balançando, nuvem de poeira, sombra, água borbulhando e
  Hidden Grotto; o Grotto entrega item ou encontro com Hidden Ability. As dez
  areas dividem as geracoes I-V em pares, excluem especies sem HA, priorizam
  variantes ainda nao possuidas e reservam 1/240 aos lendarios HA. Eventos
  lendários usam 1/512 por padrão e 1/240 somente no local especial.
- Concluído: cinco tiers de PWT liberados a cada dois Bosses, Benga na Área 10,
  Champions Tournament de cinco rodadas, tema Black e Royal Unova diário no
  Challenge Hub com calendário semanal determinístico de três a sete lutas.
- Concluído: DNA Splicers/Kyurem, `FORM` comum para os Forces of Nature,
  Drives/Genesect, Secret Sword/Keldeo, Relic Song/Meloetta, Zen Mode/Darmanitan
  e demais formas com dados de batalha e caminhos visuais próprios em Home,
  Box e Summary.
- Concluído: pack PGA3 v37 com 10.971 assets (21.758.082 bytes), incluindo 264
  recursos obrigatórios de Unova e nenhum ausente. A auditoria registrou 605
  tags visuais e 1.645 frames de animação sem frame vazio, opaco ou grande não
  autorizado.
- Validado: `scripts/test.ps1` termina em `All native tests passed.`. O firmware
  ESP32 não foi compilado, empacotado nem gravado neste lote, conforme pedido.
