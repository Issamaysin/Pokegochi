# Auditoria manual das 153 passivas

Pedido relacionado: `R20261019-167`.

Esta é uma conferência nominal, passiva por passiva. As habilidades 001–077
seguem o comportamento de FireRed/Generation III; 078–124 seguem Platinum /
Generation IV. Os IDs 128–156 são as 29 habilidades de Mega/Primal suportadas
pelo projeto e foram confrontadas com `pokeemerald-expansion` e com as decisões
de design já estabelecidas no Pokegochi. Os IDs 125–127 continuam reservados e
não representam passivas jogáveis.

Para cada entrada foram lidos gatilho, lado afetado, ordem de resolução,
imunidades, duração, chance, interação com item/clima/status, troca, Substitute,
Mold Breaker e comportamento após desmaio. A matriz automática foi executada
somente depois dessa leitura, como regressão; ela não substituiu a conferência
individual.

Fontes locais primárias:

- FireRed: `.downloads/pokefirered-tree/pokefirered-master/src/battle_util.c`,
  `src/battle_script_commands.c` e `data/battle_scripts_1.s`.
- Platinum: `.downloads/pokeplatinum-tree/src/battle/battle_lib.c`,
  `battle_script.c` e `battle_controller_player.c`.
- Habilidades posteriores usadas por Mega/Primal:
  `.downloads/pokeemerald-expansion/src/battle_util.c` e
  `src/battle_script_commands.c`.

Legenda: `OK` indica que o caminho existente correspondia à regra nominal.
`CORRIGIDO` marca uma divergência encontrada e reparada nesta auditoria.
`ADAPTADO 1x1` identifica um efeito exclusivamente de campo/doubles ou
informativo que não altera a batalha individual do Pokegochi.

## IDs 001–025 — FireRed

| ID | Passiva | Comportamento conferido | Resultado |
|---:|---|---|---|
| 001 | Stench | Efeito de campo sobre encontros por passos; o Pokegochi inicia encontros explicitamente | ADAPTADO 1x1 |
| 002 | Drizzle | Chuva permanente ao entrar; ordem por Speed quando dois climas entram juntos | OK |
| 003 | Speed Boost | Speed +1 no fim de cada turno completo após a entrada | OK |
| 004 | Battle Armor | Bloqueia crítico, inclusive antes do cálculo de dano | OK |
| 005 | Sturdy | Regra Gen III: bloqueia somente golpes OHKO | OK |
| 006 | Damp | Bloqueia Explosion/Self-Destruct de qualquer lado e impede o auto-KO | OK |
| 007 | Limber | Impede paralysis e cura a condição ao adquirir a passiva | OK |
| 008 | Sand Veil | Evasão em sandstorm e imunidade ao dano da tempestade | OK |
| 009 | Static | 1/3 após contato que realmente alcança o portador; imunidades de status | OK |
| 010 | Volt Absorb | Gen III: absorve golpe Electric com dano e recupera 1/4 do HP | OK |
| 011 | Water Absorb | Gen III: absorve golpe Water com dano e recupera 1/4 do HP | OK |
| 012 | Oblivious | Impede Attract e remove atração ao adquirir a passiva | OK |
| 013 | Cloud Nine | Suprime efeitos de clima para os dois combatentes sem apagar o clima | OK |
| 014 | Compound Eyes | Multiplicador de Accuracy na ordem inteira do FireRed | OK |
| 015 | Insomnia | Impede Sleep/Rest e é ignorável por Mold Breaker | CORRIGIDO |
| 016 | Color Change | Muda para o tipo do golpe após dano real, por acerto | OK |
| 017 | Immunity | Impede Poison/Toxic e cura ao adquirir a passiva | OK |
| 018 | Flash Fire | Cancela todo golpe Fire, inclusive Will-O-Wisp; arma bônus Fire | OK |
| 019 | Shield Dust | Bloqueia somente os códigos adicionais cobertos pelo FireRed | OK |
| 020 | Own Tempo | Impede Confusion e cura ao adquirir a passiva | OK |
| 021 | Suction Cups | Impede Roar/Whirlwind e troca forçada | OK |
| 022 | Intimidate | Attack -1 na entrada, respeitando Substitute, Mist e proteções | OK |
| 023 | Shadow Tag | Regra Gen III de aprisionamento, inclusive confronto espelhado | OK |
| 024 | Rough Skin | Dano de 1/16 por contato na Generation III | OK |
| 025 | Wonder Guard | Só permite eficácia super efetiva e preserva a exceção retail de Fire Fang | OK |

## IDs 026–050 — FireRed

| ID | Passiva | Comportamento conferido | Resultado |
|---:|---|---|---|
| 026 | Levitate | Imunidade Ground, removida por Gravity/Ingrain e ignorável por Mold Breaker | OK |
| 027 | Effect Spore | 10% no contato; escolha uniforme entre Poison, Sleep e Paralysis | OK |
| 028 | Synchronize | Reflete Poison/Burn/Paralysis; Toxic refletido vira Poison comum; efeito de campo da Nature | OK |
| 029 | Clear Body | Bloqueia quedas de stats impostas pelo oponente | OK |
| 030 | Natural Cure | Cura condição ao sair, inclusive no encerramento da batalha | OK |
| 031 | Lightning Rod | Em Gen III apenas redireciona Electric em doubles; não há alvo alternativo no 1x1 | ADAPTADO 1x1 |
| 032 | Serene Grace | Dobra chance de efeito adicional sem criar uma segunda rolagem | OK |
| 033 | Swift Swim | Dobra Speed em Rain/Heavy Rain ativa | OK |
| 034 | Chlorophyll | Dobra Speed em Sun/Harsh Sun ativa | OK |
| 035 | Illuminate | Modifica encontros por passos, sistema inexistente no Pokegochi | ADAPTADO 1x1 |
| 036 | Trace | Copia somente passivas permitidas depois dos outros efeitos de entrada | OK |
| 037 | Huge Power | Dobra Attack físico antes dos estágios | OK |
| 038 | Poison Point | 1/3 após contato, respeitando tipo, status e imunidade | OK |
| 039 | Inner Focus | Impede Flinch; não impede o Speed +1 posterior de Steadfast quando aplicável | OK |
| 040 | Magma Armor | Impede Freeze, cura ao adquirir e reduz ciclos de ovo | OK |
| 041 | Water Veil | Impede Burn e cura ao adquirir | OK |
| 042 | Magnet Pull | Prende Steel aterrado/ativo e mantém o efeito de campo previsto | OK |
| 043 | Soundproof | Bloqueia a lista nominal de golpes sonoros antes do script do golpe | OK |
| 044 | Rain Dish | Recupera 1/16 no fim do turno sob chuva | OK |
| 045 | Sand Stream | Sandstorm permanente na entrada | OK |
| 046 | Pressure | Consome PP adicional apenas quando o golpe pode afetar o lado oposto | OK |
| 047 | Thick Fat | Reduz pela metade Fire/Ice no ponto correto do cálculo | OK |
| 048 | Early Bird | Consome dois ticks de Sleep por tentativa de ação | OK |
| 049 | Flame Body | 1/3 de Burn por contato e efeito de incubação | OK |
| 050 | Run Away | Garante fuga de wild, sem atravessar restrições que anulam a fuga | OK |

## IDs 051–077 — FireRed

| ID | Passiva | Comportamento conferido | Resultado |
|---:|---|---|---|
| 051 | Keen Eye | Impede queda de Accuracy imposta pelo adversário | OK |
| 052 | Hyper Cutter | Impede queda de Attack imposta pelo adversário | OK |
| 053 | Pickup | Sorteio pós-batalha PvE; não vaza de passiva copiada nem roda em link | OK |
| 054 | Truant | Alterna ação/loaf por relógio local e reinicia ao entrar/adquirir | OK |
| 055 | Hustle | Bônus físico e penalidade de Accuracy na regra Gen III adotada | OK |
| 056 | Cute Charm | 1/3 de atração por contato, com gênero e imunidades; efeito de campo | OK |
| 057 | Plus | Exige aliado com Minus em doubles | ADAPTADO 1x1 |
| 058 | Minus | Exige aliado com Plus em doubles | ADAPTADO 1x1 |
| 059 | Forecast | Forma/tipo de Castform por clima; volta a Normal ao suprimir/perder a passiva | CORRIGIDO |
| 060 | Sticky Hold | Impede Thief/Trick/Knock Off e anuncia a passiva | OK |
| 061 | Shed Skin | 1/3 de cura de status no fim do turno | OK |
| 062 | Guts | Attack ×1,5 sob qualquer major status e neutraliza a redução de Burn | OK |
| 063 | Marvel Scale | Defense ×1,5 sob major status | OK |
| 064 | Liquid Ooze | Inverte todo dreno de HP, inclusive Dream Eater e Leech Seed | OK |
| 065 | Overgrow | Grass ×1,5 em HP igual ou abaixo de 1/3 | OK |
| 066 | Blaze | Fire ×1,5 em HP igual ou abaixo de 1/3 | OK |
| 067 | Torrent | Water ×1,5 em HP igual ou abaixo de 1/3 | OK |
| 068 | Swarm | Bug ×1,5 em HP igual ou abaixo de 1/3 | OK |
| 069 | Rock Head | Bloqueia recoil comum, nunca o recoil especial de Struggle | OK |
| 070 | Drought | Sun permanente na entrada | OK |
| 071 | Arena Trap | Prende somente alvo aterrado, considerando Flying, Levitate, Ingrain, Gravity e Magnet Rise | OK |
| 072 | Vital Spirit | Impede Sleep/Rest, efeito de campo e interação correta com Mold Breaker | CORRIGIDO |
| 073 | White Smoke | Bloqueia quedas de stats impostas pelo adversário | OK |
| 074 | Pure Power | Dobra Attack físico antes dos estágios | OK |
| 075 | Shell Armor | Bloqueia críticos | OK |
| 076 | Cacophony | Mesmo bloqueio nominal de som de Soundproof | OK |
| 077 | Air Lock | Suprime efeitos de clima para ambos sem apagar o clima | OK |

## IDs 078–100 — Platinum

| ID | Passiva | Comportamento conferido | Resultado |
|---:|---|---|---|
| 078 | Tangled Feet | Evasão ×2 enquanto o portador está confuso | OK |
| 079 | Motor Drive | Cancela qualquer golpe Electric, inclusive Thunder Wave, e eleva Speed +1 | CORRIGIDO |
| 080 | Rivalry | Poder ×1,25 para mesmo gênero e ×0,75 para oposto; neutro em genderless | OK |
| 081 | Steadfast | Speed +1 quando Flinch realmente impede a ação | OK |
| 082 | Snow Cloak | Accuracy adversária ×0,8 e imunidade ao dano de Hail | CORRIGIDO |
| 083 | Gluttony | Antecipação de pinch Berry de 1/4 para 1/2 do HP | OK |
| 084 | Anger Point | Maximiza Attack após crítico que cause HP damage, somente se sobreviver | CORRIGIDO |
| 085 | Unburden | Latch local ao entrar/adquirir item; dobra Speed após perder/consumir; reseta na troca e não passa por Baton Pass | CORRIGIDO |
| 086 | Heatproof | Metade de dano Fire e metade do residual de Burn | OK |
| 087 | Simple | Dobra o efeito do estágio durante cálculo sem duplicar o valor armazenado | CORRIGIDO |
| 088 | Dry Skin | Cura 1/8 em Rain, perde 1/8 em Sun, Water cura e Fire causa ×1,25 | OK |
| 089 | Download | Ignora alvo sob Substitute; compara Defense/Sp. Def efetivas e eleva Sp. Atk no empate | CORRIGIDO |
| 090 | Iron Fist | Lista Platinum completa, incluindo Dynamic Punch e Hammer Arm; bônus ×1,2 | CORRIGIDO |
| 091 | Poison Heal | Cura 1/8 no lugar do dano de Poison/Toxic | OK |
| 092 | Adaptability | STAB passa de ×1,5 para ×2 | OK |
| 093 | Skill Link | Multi-hit variável sempre usa cinco acertos | OK |
| 094 | Hydration | Cura major status no fim do turno sob Rain | OK |
| 095 | Solar Power | Sp. Atk ×1,5 no sol e perda de 1/8 no fim do turno | OK |
| 096 | Quick Feet | Speed ×1,5 sob status e não sofre a redução de Paralysis | OK |
| 097 | Normalize | Converte golpes para Normal sem transformar Struggle em golpe tipado | OK |
| 098 | Sniper | Crítico passa de ×2 para ×3 na regra Platinum | OK |
| 099 | Magic Guard | Impede residual, hazards, recoil/crash comum, Aftermath e Innards Out; Struggle continua causando recoil | CORRIGIDO |
| 100 | No Guard | Golpes usados por/contra o portador ignoram a rolagem comum de Accuracy | OK |

## IDs 101–124 — Platinum

| ID | Passiva | Comportamento conferido | Resultado |
|---:|---|---|---|
| 101 | Stall | Portador age por último dentro da mesma prioridade, respeitando Trick Room/ordens especiais | OK |
| 102 | Technician | ×1,5 depois de resolver poder variável se ele for ≤60; Struggle excluído | CORRIGIDO |
| 103 | Leaf Guard | Impede major status no sol, incluindo Sleep direto e as duas fases de Yawn; Mold Breaker atravessa o golpe direto | CORRIGIDO |
| 104 | Klutz | Suprime o efeito do próprio item sem apagar sua presença para troca/roubo | OK |
| 105 | Mold Breaker | Allow-list nominal de passivas quebráveis em dano, Accuracy e efeitos | CORRIGIDO |
| 106 | Super Luck | +1 estágio de crítico | OK |
| 107 | Aftermath | 1/4 após KO por contato; Damp, Magic Guard e U-turn impedem a ativação | CORRIGIDO |
| 108 | Anticipation | Aviso de golpe super efetivo/OHKO; seleção aberta já expõe o risco e não há efeito mecânico | ADAPTADO 1x1 |
| 109 | Forewarn | Aviso do golpe de maior poder; ficha de golpes já é inspecionável e não há efeito mecânico | ADAPTADO 1x1 |
| 110 | Unaware | Ignora estágios ofensivos/defensivos do adversário no lado correto do cálculo | OK |
| 111 | Tinted Lens | Dobra dano quando a eficácia final é menor que ×1 | OK |
| 112 | Filter | Reduz em 25% dano super efetivo | OK |
| 113 | Slow Start | Attack/Speed pela metade por cinco turnos completos; relógio reinicia ao entrar/adquirir | OK |
| 114 | Scrappy | Normal/Fighting atravessam somente a imunidade de Ghost | OK |
| 115 | Storm Drain | Regra Gen IV: apenas redireciona Water em doubles; não dá imunidade nem Sp. Atk | CORRIGIDO / ADAPTADO 1x1 |
| 116 | Ice Body | Imunidade ao dano de Hail e cura de 1/16 sob Hail | CORRIGIDO |
| 117 | Solid Rock | Reduz em 25% dano super efetivo | OK |
| 118 | Snow Warning | Hail permanente na entrada | OK |
| 119 | Honey Gather | Recompensa de Honey fora de batalha; item/loop de Honey não existe neste inventário | ADAPTADO 1x1 |
| 120 | Frisk | Aviso informativo do item adversário; identidade continua oculta até interação de item | ADAPTADO 1x1 |
| 121 | Reckless | ×1,2 na família de recoil/crash; Struggle excluído | CORRIGIDO |
| 122 | Multitype | Plate define tipo/forma de Arceus e impede manipulação indevida do item/passiva | OK |
| 123 | Flower Gift | Attack/Sp. Def ×1,5 no sol, ignorável no defensor por Mold Breaker; Cherrim usa forma Sunshine oficial | CORRIGIDO |
| 124 | Bad Dreams | Remove 1/8 do HP do adversário adormecido no fim do turno | OK |

## IDs 128–156 — Mega, Primal e extensões

| ID | Passiva | Comportamento conferido | Resultado |
|---:|---|---|---|
| 128 | Tough Claws | ×1,3 em golpes com flag de contato | OK |
| 129 | Mega Launcher | ×1,5 nos golpes pulse/aura existentes até a Gen IV | OK |
| 130 | Adaptability | Mesmo STAB ×2 da habilidade 092 | OK |
| 131 | No Guard | Mesmo acerto garantido da habilidade 100 | OK |
| 132 | Parental Bond | Segundo acerto somente em golpe elegível, com 25% do dano no padrão moderno adotado | OK |
| 133 | Aerilate | Normal vira Flying e recebe bônus de 20% conforme regra adotada do projeto | OK |
| 134 | Mold Breaker | Mesmo núcleo nominal da habilidade 105 | CORRIGIDO |
| 135 | Steadfast | Mesmo gatilho da habilidade 081 | OK |
| 136 | Sand Force | ×1,3 para Rock/Ground/Steel durante Sandstorm | OK |
| 137 | Technician | Mesmo limite dinâmico e exclusão de Struggle da habilidade 102 | CORRIGIDO |
| 138 | Skill Link | Mesmo fechamento em cinco acertos da habilidade 093 | OK |
| 139 | Solar Power | Mesmo Sp. Atk/HP da habilidade 095 | OK |
| 140 | Pixilate | Normal vira Fairy e recebe bônus de 20% conforme regra adotada | OK |
| 141 | Magic Bounce | Reflete golpes de status refletíveis e preserva autoria/alvo do evento | OK |
| 142 | Filter | Mesmo redutor super efetivo da habilidade 112 | OK |
| 143 | Strong Jaw | ×1,5 em Bite, Crunch, Hyper Fang, Poison Fang e três elemental Fangs | OK |
| 144 | Sheer Force | ×1,3 e suprime efeito adicional e flinch de King's Rock | CORRIGIDO |
| 145 | Prankster | +1 somente para categoria Status; Dark adversário é imune à prioridade moderna | CORRIGIDO |
| 146 | Refrigerate | Normal vira Ice e recebe bônus de 20% conforme regra adotada | OK |
| 147 | Primordial Sea | Heavy Rain enquanto houver fonte; Fire falha e clima comum não substitui | OK |
| 148 | Desolate Land | Harsh Sun enquanto houver fonte; Water falha e clima comum não substitui | OK |
| 149 | Delta Stream | Strong Winds remove apenas fraqueza do componente Flying | OK |
| 150 | Multiscale | Metade do dano em HP cheio e interação correta com Mold Breaker | CORRIGIDO |
| 151 | Innards Out | Devolve somente HP perdido no golpe final; Magic Guard impede | CORRIGIDO |
| 152 | Electric Surge | Adaptação deliberada do terreno como bônus Electric local persistente no 1x1 | OK |
| 153 | Stalwart | Redirecionamento não existe em batalha individual | ADAPTADO 1x1 |
| 154 | Sharpness | ×1,5 na lista slicing oficial até Gen IV; Razor Wind removido da lista | CORRIGIDO |
| 155 | Dragonize | Normal vira Dragon e recebe bônus de 20% conforme regra própria | OK |
| 156 | Mega Sol | Sol pessoal somente para o portador; não altera clima nem beneficia o adversário | CORRIGIDO |

## Divergências reparadas

- Estado e troca de item: Unburden agora cobre Berry persistente, Fling/Natural
  Gift, Knock Off, Thief, Trick e Recycle, reseta ao trocar e não passa por
  Baton Pass.
- Estágios: Simple passou a alterar a leitura efetiva, não o valor salvo; isso
  vale para dano, Speed, Accuracy/Evasion, confusão e golpes atrasados.
- Clima/formas: Snow Cloak e Ice Body ficaram imunes a Hail; Flower Gift ganhou
  forma Sunshine oficial de Cherrim e Castform também reverte ao suprimir
  Forecast.
- Absorção/entrada: Motor Drive aceita golpes Electric sem dano; Download usa
  estágios, empate e Substitute corretos; Storm Drain perdeu a regra moderna
  incorreta de imunidade/+Sp. Atk.
- Dano/contato: listas de Iron Fist e Sharpness corrigidas; Technician e
  Reckless não amplificam Struggle; Magic Guard, Anger Point, Aftermath,
  Innards Out, Sheer Force e Flower Gift/Mold Breaker foram alinhados.
- Status/prioridade: Leaf Guard cobre Sleep/Yawn; Prankster usa a categoria
  Status em vez de `power == 0` e respeita a imunidade Dark adotada.

Cobertura dedicada: `test/native/test_ability_matrix.cpp`. A checagem de
inventário complementar permanece em `scripts/audit_abilities.py` e falha se
uma nova passiva entrar sem classificação explícita.
