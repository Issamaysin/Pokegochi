# Auditoria manual dos 467 golpes

Pedido relacionado: `R20261017-164`.

Esta é uma conferência manual, golpe por golpe. Os IDs 001–354 usam o
comportamento de batalha do FireRed local; os IDs 355–467 usam o Platinum
local. Por decisão já existente do projeto, a classe físico/especial de todos
os 467 golpes vem do Platinum; para os IDs 001–354, os demais dados, scripts e
efeitos continuam sendo os do FireRed. Adaptações inevitáveis de batalha 1x1
são anotadas explicitamente.
Testes automatizados entram somente depois da leitura das regras e servem como
regressão: eles não substituem a decisão registrada nesta tabela.

Nota de fidelidade do RNG: para IDs 001–354, a chance adicional registrada na
tabela do FireRed é resolvida pelo comparador inclusivo original
`Random() % 100 <= chance`; para IDs 355–467 permanece a regra do Platinum.
Golpes de status primário não fazem uma segunda rolagem sintética de 100% após
passarem pela própria precisão.

Legenda: `OK` significa que dados, pré-condições, acerto, efeito, duração e
falha foram lidos no jogo-fonte e comparados com o caminho correspondente do
motor. `CORRIGIDO` identifica uma divergência encontrada nesta auditoria.

## IDs 001–025

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 001 | Pound | FireRed `EFFECT_HIT`; dano Normal comum, contato e Protect | OK |
| 002 | Karate Chop | `EFFECT_HIGH_CRITICAL`; +1 estágio crítico | OK |
| 003 | Double Slap | `EFFECT_MULTI_HIT`; 2–5 acertos na distribuição 3/8, 3/8, 1/8, 1/8 | OK |
| 004 | Comet Punch | Mesmo laço e distribuição original de multi-hit | OK |
| 005 | Mega Punch | `EFFECT_HIT`; dano/precisão/PP e flags originais | OK |
| 006 | Pay Day | Dano normal; ao acertar pelo jogador acumula `nível * 5`, limitado a 65535 | OK |
| 007 | Fire Punch | 10% de burn após dano; imunidades, Substitute e Shield Dust | OK |
| 008 | Ice Punch | 10% de freeze após dano; imunidades, clima, Substitute e Shield Dust | OK |
| 009 | Thunder Punch | 10% de paralysis após dano; imunidades, Substitute e Shield Dust | OK |
| 010 | Scratch | `EFFECT_HIT`; dano e flags originais | OK |
| 011 | Vice Grip | `EFFECT_HIT`; dano e flags originais | OK |
| 012 | Guillotine | OHKO: regra de nível, Sturdy, precisão `30 + delta de nível` e dano igual ao HP atual | OK |
| 013 | Razor Wind | Dois turnos e um único gasto de PP; FireRed não o inclui no bônus crítico | OK |
| 014 | Swords Dance | Alvo usuário, Attack +2, falha no limite e compatibilidade com Snatch | OK |
| 015 | Cut | `EFFECT_HIT`; dano/precisão/PP e flags originais | OK |
| 016 | Gust | Atinge Fly/Bounce e dobra o poder contra esses estados | OK |
| 017 | Wing Attack | `EFFECT_HIT`; dano e flags originais | OK |
| 018 | Whirlwind | Prioridade -6; Suction Cups/Ingrain; teste original de nível; fuga wild ou troca trainer | OK |
| 019 | Fly | Turno de subida, semi-invulnerabilidade, alcance especial de Gust/Thunder/Twister/Sky Uppercut e ataque no segundo turno | OK |
| 020 | Bind | 3–6 turnos; impede troca/fuga; dano residual de 1/16 enquanto ainda preso | OK |
| 021 | Slam | `EFFECT_HIT`; dano/precisão/PP e flags originais | OK |
| 022 | Vine Whip | `EFFECT_HIT`; dano/precisão/PP e flags originais | OK |
| 023 | Stomp | 30% de flinch; poder dobrado contra alvo que usou Minimize | OK |
| 024 | Double Kick | Exatamente dois acertos, com resolução individual de dano | OK |
| 025 | Mega Kick | `EFFECT_HIT`; dano/precisão/PP e flags originais | OK |

## IDs 026–050

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 026 | Jump Kick | FireRed `EFFECT_RECOIL_IF_MISS`; crash usa metade do dano calculado, limitado a metade do HP máximo do alvo, e não ocorre em imunidade; Gravity bloqueia o golpe em Gen IV | OK |
| 027 | Rolling Kick | `EFFECT_FLINCH_HIT`; 30% de flinch somente se o alvo ainda não agiu, respeitando Substitute, Shield Dust e Inner Focus | OK |
| 028 | Sand-Attack | `EFFECT_ACCURACY_DOWN`; Accuracy -1 no alvo, com acerto, Substitute, Mist e proteções de habilidade | OK |
| 029 | Headbutt | Mesmo caminho de flinch de 30% do FireRed, com contato | OK |
| 030 | Horn Attack | `EFFECT_HIT`; dano/precisão/PP e flags originais | OK |
| 031 | Fury Attack | `EFFECT_MULTI_HIT`; 2–5 golpes na distribuição 3/8, 3/8, 1/8, 1/8 | OK |
| 032 | Horn Drill | OHKO com regra de nível, Sturdy e precisão `30 + diferença de nível` | OK |
| 033 | Tackle | `EFFECT_HIT`; valores e flags do FireRed | OK |
| 034 | Body Slam | Dano e 30% de paralysis; imunidades, Substitute, Shield Dust e Safeguard conferidos | OK |
| 035 | Wrap | Dano seguido de aprisionamento por 3–6 contagens; não reaplica enquanto ativo e não atravessa Substitute | OK |
| 036 | Take Down | Recuo de 1/4 do dano real; Rock Head bloqueia o recuo | OK |
| 037 | Thrash | Trava por 2–3 turnos e confunde ao final, salvo Own Tempo | OK |
| 038 | Double-Edge | Recuo de 1/3 do dano real em Gen III; Rock Head bloqueia | OK |
| 039 | Tail Whip | Defense -1 no alvo; alvo duplo do cartucho adaptado ao único oponente do 1x1 | OK |
| 040 | Poison Sting | 30% de poison após dano; tipos, Immunity, Safeguard, Substitute e Shield Dust | OK |
| 041 | Twineedle | Exatamente dois acertos; cada acerto resolve dano e o efeito de poison usa a chance original de 20% | OK |
| 042 | Pin Missile | `EFFECT_MULTI_HIT`; 2–5 acertos com distribuição original | OK |
| 043 | Leer | Defense -1; alvo duplo adaptado ao oponente único | OK |
| 044 | Bite | Dano Dark e 30% de flinch; classe física do Platinum conforme o split global deliberado do projeto | OK |
| 045 | Growl | Attack -1 e bloqueio por Soundproof/Cacophony; alvo duplo adaptado ao 1x1 | OK |
| 046 | Roar | Prioridade -6; regra de nível, Suction Cups e Ingrain; encerra wild ou força troca em trainer | OK |
| 047 | Sing | Sleep primário; Soundproof, Substitute, Sleep existente, Insomnia/Vital Spirit, Uproar e outro status são resolvidos na ordem anterior à precisão do script; Safeguard vem depois | CORRIGIDO |
| 048 | Supersonic | Confusion de 2–5 turnos; Soundproof/Cacophony, Own Tempo, Safeguard e Substitute | OK |
| 049 | Sonic Boom | Dano fixo 20, ainda sujeito à imunidade de tipo | OK |
| 050 | Disable | Usa o último golpe do alvo, exige PP restante e aplica 2–5 turnos; falha sob Substitute ou se já houver Disable | OK |

## IDs 051–075

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 051 | Acid | FireRed reduz Defense, não Sp. Def.; 10% após dano, não atravessa Substitute e não é bloqueado por Shield Dust | CORRIGIDO |
| 052 | Ember | Dano Fire e 10% de burn com todas as imunidades e proteções | OK |
| 053 | Flamethrower | Dano Fire e 10% de burn; dados originais | OK |
| 054 | Mist | Protege o lado do usuário de quedas impostas pelo oponente por cinco turnos e falha se já ativo | OK |
| 055 | Water Gun | `EFFECT_HIT`; dados e flags originais | OK |
| 056 | Hydro Pump | `EFFECT_HIT`; dados e flags originais | OK |
| 057 | Surf | Atinge alvo em Dive e dobra o poder nesse estado; alvo amplo do cartucho reduzido ao oponente no 1x1 | OK |
| 058 | Ice Beam | Dano Ice e 10% de freeze, incluindo bloqueio por tipo, clima e Magma Armor | OK |
| 059 | Blizzard | 70% de precisão no conjunto de regras FireRed e 10% de freeze; alvo duplo adaptado ao 1x1 | OK |
| 060 | Psybeam | 10% de confusion após dano; Substitute, Shield Dust, Own Tempo e Safeguard | OK |
| 061 | Bubble Beam | 10% de Speed -1 após dano; Shield Dust não bloqueia quedas secundárias de stats no FireRed | CORRIGIDO |
| 062 | Aurora Beam | 10% de Attack -1 após dano; Shield Dust não bloqueia quedas secundárias de stats no FireRed | CORRIGIDO |
| 063 | Hyper Beam | Recarrega depois de acertar ou ser bloqueado por Protect/Detect; miss real, imunidade ou absorção não armam recarga | CORRIGIDO |
| 064 | Peck | `EFFECT_HIT`; dano e flags originais | OK |
| 065 | Drill Peck | `EFFECT_HIT`; dano e flags originais | OK |
| 066 | Submission | Recuo de 1/4 do dano real e supressão por Rock Head | OK |
| 067 | Low Kick | Poder por peso nas seis faixas do FireRed: 20/40/60/80/100/120 | OK |
| 068 | Counter | Prioridade -5; devolve 2x o dano físico recebido no turno, respeita imunidade Fighting e falha sem dano válido | OK |
| 069 | Seismic Toss | Dano igual ao nível do usuário, ainda sujeito à imunidade de tipo | OK |
| 070 | Strength | `EFFECT_HIT`; dados e flags originais | OK |
| 071 | Absorb | Cura metade do dano realmente removido do HP, sem drenar Substitute; Liquid Ooze inverte a cura | OK |
| 072 | Mega Drain | Mesmo caminho de drenagem de 1/2 do FireRed | OK |
| 073 | Leech Seed | Falha em Grass, alvo já semeado ou Substitute; drena 1/8 e cura somente o HP realmente removido, com Liquid Ooze | OK |
| 074 | Growth | Sp. Atk +1 no conjunto de regras FireRed | OK |
| 075 | Razor Leaf | +1 estágio crítico; alvo duplo adaptado ao 1x1 | OK |

## IDs 076–100

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 076 | Solar Beam | Carrega um turno; Sun elimina a carga e Rain/Sandstorm/Hail reduzem o dano pela metade | OK |
| 077 | Poison Powder | Poison primário; tipo Poison/Steel, Immunity, estado existente, Safeguard e Substitute | OK |
| 078 | Stun Spore | Paralysis primária e `typecalc` do script FireRed antes da precisão | OK |
| 079 | Sleep Powder | Mesmo script ordenado de Sleep, sem rolagem adicional de efeito depois da precisão | CORRIGIDO |
| 080 | Petal Dance | Rampage de 2–3 turnos e confusion no fim, salvo Own Tempo | OK |
| 081 | String Shot | Speed -1; alvo duplo adaptado ao oponente único | OK |
| 082 | Dragon Rage | Dano fixo 40, ainda sujeito à imunidade de tipo | OK |
| 083 | Fire Spin | Dano e aprisionamento de 3–6 contagens | OK |
| 084 | Thunder Shock | 10% de paralysis após dano | OK |
| 085 | Thunderbolt | 10% de paralysis após dano | OK |
| 086 | Thunder Wave | `typecalc` impede Electric contra Ground; Limber, estado existente, Safeguard e Substitute | OK |
| 087 | Thunder | Rain garante acerto, Sun usa 50 de precisão, alcança Fly/Bounce e tem 30% de paralysis | OK |
| 088 | Rock Throw | `EFFECT_HIT`; dados e flags originais | OK |
| 089 | Earthquake | Atinge Dig e dobra o poder contra alvo subterrâneo | OK |
| 090 | Fissure | OHKO com regra de nível, Sturdy, precisão especial e imunidade Ground | OK |
| 091 | Dig | Turno subterrâneo, semi-invulnerabilidade e ataque no segundo turno; Earthquake/Magnitude alcançam com poder dobrado | OK |
| 092 | Toxic | Badly Poisoned; Poison/Steel, Immunity, estado existente, Safeguard e Substitute | OK |
| 093 | Confusion | 10% de confusion após dano | OK |
| 094 | Psychic | 10% de Sp. Def -1 após dano; Shield Dust não bloqueia a queda | CORRIGIDO |
| 095 | Hypnosis | Mesmo script ordenado de Sleep, com precisão própria e duração 2–5 | CORRIGIDO |
| 096 | Meditate | Attack +1 no usuário | OK |
| 097 | Agility | Speed +2 no usuário | OK |
| 098 | Quick Attack | Prioridade +1 e dano comum | OK |
| 099 | Rage | Ativa Rage ao acertar e eleva Attack quando o usuário perde HP para golpe adversário; encerra ao escolher outro golpe | OK |
| 100 | Teleport | Escapa apenas de batalha wild e falha sob trapping, Ingrain ou habilidade de aprisionamento | OK |

## IDs 101–125

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 101 | Night Shade | Dano igual ao nível do usuário e imunidade Ghost contra Normal | OK |
| 102 | Mimic | Copia o último golpe permitido do alvo no próprio slot, com no máximo 5 PP; falha para lista proibida, duplicata, Transform e Substitute | OK |
| 103 | Screech | Defense -2 e bloqueio por Soundproof/Cacophony | OK |
| 104 | Double Team | Evasion +1 no usuário | OK |
| 105 | Recover | Cura metade do HP máximo e falha com HP cheio | OK |
| 106 | Harden | Defense +1 no usuário | OK |
| 107 | Minimize | Evasion +1 e ativa a marca usada pelos golpes que dobram contra Minimize | OK |
| 108 | Smokescreen | Accuracy -1 no alvo | OK |
| 109 | Confuse Ray | Confusion primária de 2–5 turnos com os bloqueios originais | OK |
| 110 | Withdraw | Defense +1 no usuário | OK |
| 111 | Defense Curl | Defense +1 e ativa a duplicação de Rollout/Ice Ball, mesmo que o estágio já esteja no máximo | OK |
| 112 | Barrier | Defense +2 no usuário | OK |
| 113 | Light Screen | Reduz dano especial pela metade por cinco turnos e falha se o lado já estiver protegido | OK |
| 114 | Haze | Zera os sete estágios dos dois combatentes, sem limpar outros voláteis | OK |
| 115 | Reflect | Reduz dano físico pela metade por cinco turnos e falha se já ativo | OK |
| 116 | Focus Energy | +2 estágios de crítico, passável por Baton Pass; uso repetido falha | OK |
| 117 | Bide | Armazena somente dano real recebido por dois turnos e devolve 2x; não atravessa Substitute e respeita imunidade Normal/Ghost com Foresight/Scrappy | CORRIGIDO |
| 118 | Metronome | Sorteia IDs 1–354 com o mesmo `Random() & 0x1FF`, rejeitando exatamente a tabela proibida do FireRed | OK |
| 119 | Mirror Move | Chama o último golpe do alvo apenas quando a flag `Mirror Move affected` permite | OK |
| 120 | Self-Destruct | Usuário desmaia, Defense do alvo é reduzida pela metade no cálculo e Damp bloqueia a explosão | OK |
| 121 | Egg Bomb | `EFFECT_HIT`; dados e flags originais | OK |
| 122 | Lick | 30% de paralysis após dano | OK |
| 123 | Smog | 40% de poison após dano | OK |
| 124 | Sludge | 30% de poison após dano | OK |
| 125 | Bone Club | 10% de flinch após dano | OK |

## IDs 126–150

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 126 | Fire Blast | 10% de burn após dano; dados originais | OK |
| 127 | Waterfall | No FireRed é dano comum, sem flinch | OK |
| 128 | Clamp | Dano e aprisionamento por 3–6 contagens | OK |
| 129 | Swift | Ignora a rolagem de precisão/evasão, mas não alcança estados semi-invulneráveis | OK |
| 130 | Skull Bash | Defense +1 no turno de preparação e dano no segundo turno, com um único gasto de PP | OK |
| 131 | Spike Cannon | Multi-hit de 2–5 com a distribuição original | OK |
| 132 | Constrict | 10% de Speed -1 após dano; Shield Dust não bloqueia a queda | CORRIGIDO |
| 133 | Amnesia | Sp. Def +2 no usuário | OK |
| 134 | Kinesis | Accuracy -1 no alvo | OK |
| 135 | Soft-Boiled | Cura metade do HP máximo e falha com HP cheio | OK |
| 136 | Hi Jump Kick | Crash de metade do dano calculado, limitado a metade do HP máximo do alvo; Gravity bloqueia | OK |
| 137 | Glare | Paralysis primária; por ser Normal no FireRed, `typecalc` impede o uso contra Ghost não identificado | OK |
| 138 | Dream Eater | Exige alvo dormindo, causa dano e recupera metade do dano real; Liquid Ooze inverte | OK |
| 139 | Poison Gas | Poison primário, 55% de precisão e bloqueios originais | OK |
| 140 | Barrage | Multi-hit de 2–5 com distribuição original | OK |
| 141 | Leech Life | Recupera metade do dano real, sem drenar Substitute; Liquid Ooze inverte | OK |
| 142 | Lovely Kiss | Mesmo script ordenado de Sleep e uma única rolagem de precisão | CORRIGIDO |
| 143 | Sky Attack | Prepara um turno; no acerto tem +1 estágio crítico e 30% de flinch | OK |
| 144 | Transform | Copia espécie/forma, habilidade, tipos, golpes com até 5 PP e sete estágios; preserva HP, status, item e personalidade e falha nas condições originais | OK |
| 145 | Bubble | 10% de Speed -1, não bloqueado por Shield Dust; alvo duplo adaptado ao 1x1 | CORRIGIDO |
| 146 | Dizzy Punch | 20% de confusion após dano | OK |
| 147 | Spore | Mesmo script ordenado de Sleep; o valor 100 de precisão ainda passa pelo comando original, sem segunda chance sintética | CORRIGIDO |
| 148 | Flash | Accuracy -1 no alvo | OK |
| 149 | Psywave | Dano entre 50% e 150% do nível usando a mesma rejeição `Random() % 16 > 10` | OK |
| 150 | Splash | Executa e produz explicitamente o resultado sem efeito | OK |

## IDs 151–175

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 151 | Acid Armor | Defense +2 no usuário | OK |
| 152 | Crabhammer | +1 estágio crítico | OK |
| 153 | Explosion | Usuário desmaia, Defense é dividida por dois no cálculo e Damp impede o golpe | OK |
| 154 | Fury Swipes | Multi-hit de 2–5 com distribuição original | OK |
| 155 | Bonemerang | Exatamente dois acertos com resolução individual | OK |
| 156 | Rest | Exige HP faltante, cura totalmente, substitui o status por Sleep e impõe os dois turnos de sono do Rest | OK |
| 157 | Rock Slide | 30% de flinch; alvo duplo adaptado ao 1x1 | OK |
| 158 | Hyper Fang | 10% de flinch após dano | OK |
| 159 | Sharpen | Attack +1 no usuário | OK |
| 160 | Conversion | Escolhe entre os quatro slots reais com a mesma rejeição RNG e muda para um tipo de golpe ainda diferente do usuário | OK |
| 161 | Tri Attack | Primeiro rola os 20%; somente depois escolhe igualmente Burn, Freeze ou Paralysis | OK |
| 162 | Super Fang | Remove metade do HP atual, arredondando para ao menos 1, e respeita imunidade Normal | OK |
| 163 | Slash | +1 estágio crítico | OK |
| 164 | Substitute | Custa 1/4 do HP máximo, exige HP estritamente maior que o custo, encerra Wrap e absorve cada golpe sem vazamento de dano | OK |
| 165 | Struggle | Dano sem tipo, recuo de 1/4 e Rock Head não bloqueia esse recuo | OK |
| 166 | Sketch | Copia permanentemente o último golpe válido no slot de Sketch; rejeita Struggle, Sketch, duplicata e Substitute | OK |
| 167 | Triple Kick | Faz nova rolagem de acerto em cada golpe e usa poderes 10, 20 e 30, parando no primeiro miss/imunidade | OK |
| 168 | Thief | Rouba somente com slot livre e item transferível; Substitute e Sticky Hold bloqueiam, Sticky Hold anuncia antes do teste do slot e NPC comum não remove o item persistente do jogador | CORRIGIDO |
| 169 | Spider Web | Impede fuga/troca e falha contra Substitute ou alvo já preso; usa `NO_ACC_CALC`, sem rolagem comum de precisão/evasão | CORRIGIDO |
| 170 | Mind Reader | Garante o próximo acerto do usuário no alvo e a relação dura a janela original | OK |
| 171 | Nightmare | Exige alvo dormindo e sem Substitute/Nightmare; remove 1/4 do HP por turno e termina ao acordar | OK |
| 172 | Flame Wheel | 10% de burn e descongela o usuário quando o golpe é executado com efeito | OK |
| 173 | Snore | Só funciona durante Sleep, é sonoro e tem 30% de flinch | OK |
| 174 | Curse | Ghost sacrifica metade do HP e amaldiçoa o alvo; demais tipos recebem Speed -1, Attack +1 e Defense +1 | OK |
| 175 | Flail | Poder 20/40/80/100/150/200 nas seis faixas exatas de HP do FireRed | OK |

## IDs 176–200

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 176 | Conversion 2 | Usa o tipo do último golpe que acertou e sorteia um tipo resistente ainda diferente do usuário; falha sem golpe válido | OK |
| 177 | Aeroblast | +1 estágio crítico | OK |
| 178 | Cotton Spore | Speed -2 no alvo | OK |
| 179 | Reversal | Mesmas seis faixas de poder 20/40/80/100/150/200 de Flail | OK |
| 180 | Spite | Retira aleatoriamente 2–5 PP do último golpe; falha com 0/1 PP e cancela a sequência forçada se zerar | OK |
| 181 | Powder Snow | 10% de freeze; alvo duplo adaptado ao 1x1 | OK |
| 182 | Protect | Protege no turno e usa a cadeia 1, 1/2, 1/4, 1/8, mantendo 1/8; falha se o adversário já agiu | OK |
| 183 | Mach Punch | Prioridade +1 e dano comum | OK |
| 184 | Scary Face | Speed -2 no alvo | OK |
| 185 | Faint Attack | Ignora a rolagem de precisão/evasão, sem ignorar semi-invulnerabilidade | OK |
| 186 | Sweet Kiss | Confusion primária com precisão e bloqueios originais | OK |
| 187 | Belly Drum | Exige mais de metade do HP e Attack abaixo do máximo; paga metade do HP máximo e leva Attack a +6 | OK |
| 188 | Sludge Bomb | 30% de poison após dano | OK |
| 189 | Mud-Slap | Accuracy -1 garantido após dano; Substitute/Mist/Abilities de stat protegem, mas Shield Dust não | CORRIGIDO |
| 190 | Octazooka | 50% de Accuracy -1 após dano; Shield Dust não bloqueia a queda | CORRIGIDO |
| 191 | Spikes | Até três camadas; entrada terrestre causa 1/8, 1/6 ou 1/4 do HP máximo | OK |
| 192 | Zap Cannon | Paralysis garantida após o acerto | OK |
| 193 | Foresight | Marca o alvo, ignora Evasion e permite Normal/Fighting contra Ghost; reaplicação continua válida como no FireRed | OK |
| 194 | Destiny Bond | Se o usuário for nocauteado pelo golpe adversário enquanto ativo, leva o atacante junto | OK |
| 195 | Perish Song | Aplica contagem aos dois combatentes, respeitando Soundproof/Cacophony individualmente, e nocauteia em zero | OK |
| 196 | Icy Wind | Speed -1 garantido após dano, não bloqueado por Shield Dust; alvo duplo adaptado ao 1x1 | CORRIGIDO |
| 197 | Detect | Mesmo encadeamento e regra de Protect | OK |
| 198 | Bone Rush | Multi-hit de 2–5 com distribuição original | OK |
| 199 | Lock-On | Garante o próximo acerto do usuário no alvo; Substitute bloqueia | OK |
| 200 | Outrage | Rampage de 2–3 turnos e confusion ao final, salvo Own Tempo | OK |

## IDs 201–225

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 201 | Sandstorm | Define clima por cinco turnos; dano residual, imunidades de tipo/Ability e supressão climática conferidos | OK |
| 202 | Giga Drain | Recupera metade do dano real e respeita Substitute/Liquid Ooze | OK |
| 203 | Endure | Compartilha a cadeia de sucesso de Protect e preserva 1 HP contra o golpe do turno | OK |
| 204 | Charm | Attack -2 no alvo | OK |
| 205 | Rollout | Cinco usos forçados com poder dobrando a cada acerto; Defense Curl dobra novamente; miss, Protect, imunidade de tipo ou Ability encerram contador e trava imediatamente | CORRIGIDO |
| 206 | False Swipe | Dano comum, mas nunca reduz o alvo abaixo de 1 HP | OK |
| 207 | Swagger | Attack +2 no alvo e confusion; Substitute bloqueia e Own Tempo impede somente a confusion | OK |
| 208 | Milk Drink | Cura metade do HP máximo e falha com HP cheio | OK |
| 209 | Spark | 30% de paralysis após dano | OK |
| 210 | Fury Cutter | Poder dobra por acertos consecutivos até 16x e a cadeia zera em miss, imunidade ou outra ação | OK |
| 211 | Steel Wing | 10% de Defense +1 no usuário; Shield Dust do alvo não bloqueia efeito no atacante | OK |
| 212 | Mean Look | Impede fuga/troca e falha contra Substitute ou alvo já preso; usa `NO_ACC_CALC`, sem rolagem comum de precisão/evasão | CORRIGIDO |
| 213 | Attract | Exige gêneros opostos e válidos; Substitute, Oblivious e atração existente bloqueiam | OK |
| 214 | Sleep Talk | Exige Sleep, escolhe slot real permitido sem gastar PP do chamado e respeita Disable/Torment/Taunt/Imprison/Encore/Choice | OK |
| 215 | Heal Bell | Cura o time; membros com Soundproof/Cacophony são ignorados como no comando de party-status do FireRed | OK |
| 216 | Return | Poder `friendship * 10 / 25`, inclusive poder dinâmico zero no extremo | OK |
| 217 | Present | Distribui 40/80/120 de poder e cura de 1/4 nas faixas exatas 102/76/26/52 de 256 | OK |
| 218 | Frustration | Poder `(255 - friendship) * 10 / 25`, inclusive zero no extremo | OK |
| 219 | Safeguard | Protege o lado por cinco turnos e falha se já ativo | OK |
| 220 | Pain Split | Exige alvo sem Substitute e iguala ambos à média dos HP atuais, limitada ao máximo individual | OK |
| 221 | Sacred Fire | 50% de burn e descongela o usuário ao executar com efeito | OK |
| 222 | Magnitude | Tabela ponderada 5/10/20/30/20/10/5 para magnitudes 4–10 e poder dobrado contra Dig | OK |
| 223 | Dynamic Punch | Confusion garantida após o acerto, sujeita aos bloqueios de efeito secundário | OK |
| 224 | Megahorn | `EFFECT_HIT`; dados e flags originais | OK |
| 225 | Dragon Breath | 30% de paralysis após dano | OK |

## IDs 226–250

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 226 | Baton Pass | Exige substituto disponível; passa os sete estágios e somente os voláteis previstos pelo FireRed, sem levar Disable, recharge, Wrap ou Choice lock | OK |
| 227 | Encore | Rejeita Struggle, Encore e Mirror Move; prende ao último slot válido por 3–6 contagens e termina se o golpe some ou fica sem PP | OK |
| 228 | Pursuit | Dobra o poder e age antes da troca comum ou Baton Pass nos dois sentidos; o caminho oponente→jogador que antes atingia o substituto foi corrigido | CORRIGIDO |
| 229 | Rapid Spin | Após um acerto efetivo remove Wrap, Leech Seed e hazards do próprio lado; inclui Toxic Spikes/Stealth Rock da expansão Gen IV | OK |
| 230 | Sweet Scent | Evasion -1; alvo duplo adaptado ao único oponente | OK |
| 231 | Iron Tail | 30% de Defense -1 após dano; Shield Dust não bloqueia a queda | CORRIGIDO |
| 232 | Metal Claw | 10% de Attack +1 no usuário, sem ser bloqueado por Shield Dust do alvo | OK |
| 233 | Vital Throw | Prioridade -1 e acerto garantido fora de semi-invulnerabilidade | OK |
| 234 | Morning Sun | Cura 1/2 sem clima, 2/3 no sol e 1/4 nos demais climas; falha com HP cheio | OK |
| 235 | Synthesis | Mesma tabela climática e pré-condição de Morning Sun | OK |
| 236 | Moonlight | Mesma tabela climática e pré-condição de Morning Sun | OK |
| 237 | Hidden Power | Tipo e poder 30–70 derivados dos seis IVs pela fórmula do FireRed; classe especial segue o split global Platinum | OK |
| 238 | Cross Chop | +1 estágio crítico, com dados/flags originais | OK |
| 239 | Twister | Alcança Fly/Bounce, dobra o poder nesse estado e tem 20% de flinch | OK |
| 240 | Rain Dance | Define Rain por cinco turnos, falha se já estiver chovendo e respeita clima primordial | OK |
| 241 | Sunny Day | Define Sun por cinco turnos, falha se o sol já estiver ativo e respeita clima primordial | OK |
| 242 | Crunch | 20% de Sp. Def -1 conforme o efeito FireRed, não bloqueado por Shield Dust; classe física conforme o split global Platinum | CORRIGIDO |
| 243 | Mirror Coat | Prioridade -5; devolve 2x o dano especial real recebido no turno e respeita imunidade Psychic/Dark | OK |
| 244 | Psych Up | Copia incondicionalmente os sete estágios do alvo, inclusive Accuracy/Evasion | OK |
| 245 | Extreme Speed | Prioridade +1 do FireRed e dano comum | OK |
| 246 | Ancient Power | 10% de elevar Attack, Defense, Sp. Atk, Sp. Def e Speed do usuário em um estágio | OK |
| 247 | Shadow Ball | 20% de Sp. Def -1 não bloqueado por Shield Dust e classe especial conforme o split global Platinum | CORRIGIDO |
| 248 | Future Sight | Armazena o dano-base na preparação, acerta após dois turnos completos, rola precisão/variação no impacto e ignora STAB/tipo/crítico; itens exclusivos e o split de Doom Desire foram corrigidos no cálculo compartilhado | CORRIGIDO |
| 249 | Rock Smash | 50% de Defense -1 após dano; Shield Dust não bloqueia a queda | CORRIGIDO |
| 250 | Whirlpool | Aprisiona por 3–6 contagens, alcança Dive e dobra o poder contra alvo submerso | OK |

## IDs 251–275

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 251 | Beat Up | Cada membro saudável e sem status contribui com nível/base Attack próprios contra a base Defense da espécie-alvo; não aplica STAB nem tabela de tipos | CORRIGIDO |
| 252 | Fake Out | Só conecta no primeiro turno do usuário em campo e causa flinch garantido quando o efeito secundário é permitido | OK |
| 253 | Uproar | Prende o usuário por 2–5 turnos, impede Sleep no campo e acorda combatentes adormecidos não protegidos por Soundproof/Cacophony | OK |
| 254 | Stockpile | Acumula no máximo três cargas e falha quando já está em três | OK |
| 255 | Spit Up | Exige carga, usa poder 100/200/300, ignora crítico e variação aleatória e consome as cargas inclusive quando Protect bloqueia | OK |
| 256 | Swallow | Exige carga, cura 1/4, 1/2 ou todo o HP e consome as cargas mesmo se o HP já estiver cheio | OK |
| 257 | Heat Wave | Dano Fire com 10% de burn; alvo duplo adaptado ao único oponente | OK |
| 258 | Hail | Define Hail por cinco turnos e aplica o residual de 1/16 com as imunidades pertinentes | OK |
| 259 | Torment | Impede repetir o último golpe e persiste até a saída do combatente | OK |
| 260 | Flatter | Sp. Atk +1 e confusion; falha somente se ambos já estiverem esgotados, com Substitute antes e a falha composta depois da precisão | CORRIGIDO |
| 261 | Will-O-Wisp | Ordem original restaurada: Substitute, burn existente, tipo Fire, Water Veil, outro status, precisão e Safeguard | CORRIGIDO |
| 262 | Memento | Falha sem sacrificar o usuário se Attack e Sp. Atk já estão em -6; Protect bloqueia as quedas, mas ainda nocauteia o usuário | CORRIGIDO |
| 263 | Facade | Dobra o poder com Poison, Bad Poison, Burn ou Paralysis; o corte físico de Burn não reduz Facade | OK |
| 264 | Focus Punch | Prioridade -3 e falha se o usuário sofreu dano real antes de agir; dano absorvido por Substitute não interrompe | OK |
| 265 | Smelling Salt | Dobra contra alvo paralisado e cura a paralysis após o dano; Substitute impede tanto a leitura quanto a cura | CORRIGIDO |
| 266 | Follow Me | Efeito de redirecionamento sem alvo adicional; falha explicitamente na adaptação 1x1 | OK |
| 267 | Nature Power | Tabela exata por terreno: Stun Spore, Razor Leaf, Earthquake, Hydro Pump, Surf, BubbleBeam, Rock Slide, Shadow Ball ou Swift | OK |
| 268 | Charge | Regra Gen III: fortalece somente o próximo golpe Electric, sem aumento de Sp. Def | OK |
| 269 | Taunt | Impede golpes de status pelo contador original e falha se já estiver ativo | OK |
| 270 | Helping Hand | Bônus depende de parceiro; falha explicitamente na adaptação 1x1 | OK |
| 271 | Trick | Troca itens nos dois sentidos e respeita Substitute, Sticky Hold, Multitype e itens não transferíveis | OK |
| 272 | Role Play | Copia qualquer Ability existente exceto Wonder Guard; Ability igual e Substitute não causam falha | OK |
| 273 | Wish | Vinculado à posição do lado, cura metade do HP ao fim do turno seguinte e mantém o anúncio quando o receptor já está com HP cheio | OK |
| 274 | Assist | Sorteia apenas golpes permitidos dos outros membros da equipe, usando a lista de exclusão e a seleção fixa do FireRed | OK |
| 275 | Ingrain | Cura 1/16 ao fim do turno, persiste no campo e impede troca, fuga e phazing | OK |

## IDs 276–300

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 276 | Superpower | Dano comum e Attack/Defense -1 no usuário somente quando o golpe afeta o alvo, inclusive através de Substitute | OK |
| 277 | Magic Coat | Prioridade +4, falha se agir por último e reflete os golpes marcados; bloqueios ordenados, Own Tempo, Leech Seed e Captivate refletidos foram alinhados | CORRIGIDO |
| 278 | Recycle | Só recupera o último item consumido quando o usuário está sem item e limpa o item reciclável após o uso | OK |
| 279 | Revenge | Prioridade -4 e poder dobrado apenas após dano real causado pelo alvo no mesmo turno | OK |
| 280 | Brick Break | Remove Reflect e Light Screen após acertar a precisão e antes do cálculo do próprio dano, inclusive antes da imunidade de tipo | OK |
| 281 | Yawn | Insomnia/Vital Spirit, Substitute e Safeguard são verificados antes da precisão; aplica Sleep no fim do turno seguinte e cancela sequências forçadas | CORRIGIDO |
| 282 | Knock Off | Poder 20 e remoção pós-dano bloqueada por Substitute/Sticky Hold; feedback de Sticky Hold restaurado e recompensas únicas locais continuam não removíveis | CORRIGIDO |
| 283 | Endeavor | Falha antes da precisão se o alvo não tiver mais HP; iguala o HP respeitando imunidade Normal e sem multiplicadores de fraqueza/resistência | OK |
| 284 | Eruption | Poder `150 * HP atual / HP máximo`, com mínimo 1, calculado no instante do ataque | OK |
| 285 | Skill Swap | Troca Abilities, permite uma Ability vazia e Ability igual, mas falha com ambas vazias ou Wonder Guard | OK |
| 286 | Imprison | Só arma se houver ao menos um golpe compartilhado com o oponente e falha quando já está ativo | OK |
| 287 | Refresh | Cura Poison, Bad Poison, Burn ou Paralysis e falha para qualquer outro estado | OK |
| 288 | Grudge | Fica ativo até a próxima ação/saída e zera o PP do golpe que nocautear diretamente o usuário | OK |
| 289 | Snatch | Prioridade +4, falha ao agir por último e executa no ladrão todos os golpes marcados; Heal Order e Defend Order roubados foram corrigidos | CORRIGIDO |
| 290 | Secret Power | Chance de 30% e efeito exato por terreno: status, queda de stat, confusion ou flinch conforme a tabela do FireRed | OK |
| 291 | Dive | Dois turnos, semi-invulnerabilidade submersa e alcance/poder dobrado corretos para Surf e Whirlpool | OK |
| 292 | Arm Thrust | Multi-hit de 2–5 com a distribuição original e resolução individual de cada acerto | OK |
| 293 | Camouflage | Muda os dois tipos para o tipo do terreno e falha se o usuário já possuir esse tipo | OK |
| 294 | Tail Glow | Sp. Atk +2 conforme a regra Gen III | OK |
| 295 | Luster Purge | 50% de Sp. Def -1 após dano; Shield Dust não bloqueia a queda | CORRIGIDO |
| 296 | Mist Ball | 50% de Sp. Atk -1 após dano; Shield Dust não bloqueia a queda | CORRIGIDO |
| 297 | Feather Dance | Attack -2 com os bloqueios de queda e reflexão por Magic Coat | OK |
| 298 | Teeter Dance | Em 1x1 afeta somente o adversário; Own Tempo, Substitute e confusion existente precedem a precisão, Safeguard vem depois | CORRIGIDO |
| 299 | Blaze Kick | +1 estágio crítico e 10% de burn após dano | OK |
| 300 | Mud Sport | Marca o usuário, reduz Electric no campo, falha só se o mesmo usuário repetir e é preservado por Baton Pass | OK |

## IDs 301–325

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 301 | Ice Ball | Mesmo contador de cinco acertos de Rollout, 30→60→120→240→480, dobrado novamente por Defense Curl; qualquer no-effect limpa contador e trava | CORRIGIDO |
| 302 | Needle Arm | Poder dobrado contra Minimize e flinch adicional de tabela; não ganha acerto garantido no FireRed | CORRIGIDO |
| 303 | Slack Off | Cura metade do HP máximo, falha com HP cheio e, quando roubado por Snatch, testa o HP do ladrão | CORRIGIDO |
| 304 | Hyper Voice | Dano sonoro Normal; Soundproof/Cacophony bloqueiam antes do script e o alvo duplo é adaptado ao 1x1 | OK |
| 305 | Poison Fang | 30 de chance de Badly Poisoned, não poison comum; Substitute, Shield Dust, tipos e Immunity preservados | CORRIGIDO |
| 306 | Crush Claw | 50% de Defense -1 após dano; Substitute, Mist e Abilities de stat protegem, mas Shield Dust não | CORRIGIDO |
| 307 | Blast Burn | Poder 150; recarga após acerto ou Protect, mas não após miss, imunidade ou absorção | OK |
| 308 | Hydro Cannon | Mesmo script de recarga de Blast Burn, com tipo Water | OK |
| 309 | Meteor Mash | 20 de chance de Attack +1 no próprio usuário; Shield Dust do alvo não bloqueia | CORRIGIDO |
| 310 | Astonish | Poder dobrado contra Minimize e 30 de chance de flinch após dano | CORRIGIDO |
| 311 | Weather Ball | Normal/50 sem clima; Water/Fire/Rock/Ice e poder 100 em Rain/Sun/Sandstorm/Hail; Cloud Nine/Air Lock anulam toda conversão | OK |
| 312 | Aromatherapy | Cura status da equipe inteira e, ao contrário de Heal Bell, não ignora membros com Soundproof | OK |
| 313 | Fake Tears | Sp. Def -2; precisão precede o bloqueio de Substitute/Mist/Ability, inclusive depois de reflexão por Magic Coat | CORRIGIDO |
| 314 | Air Cutter | +1 estágio crítico, alvo duplo adaptado ao oponente único | OK |
| 315 | Overheat | Sp. Atk -2 no usuário somente se o golpe afetar o alvo; o efeito `CERTAIN` não consome uma rolagem adicional | CORRIGIDO |
| 316 | Odor Sleuth | Mesmo `FORESIGHT` do ID 193: ignora Evasion e remove imunidade Ghost a Normal/Fighting, inclusive através de Substitute | OK |
| 317 | Rock Tomb | Speed -1 garantido pela chance 100 da tabela depois de dano; passa pelo RNG inclusivo original e Shield Dust não bloqueia | CORRIGIDO |
| 318 | Silver Wind | Chance de elevar Attack, Defense, Sp. Atk, Sp. Def e Speed do usuário; Shield Dust do alvo não bloqueia | CORRIGIDO |
| 319 | Metal Sound | Golpe sonoro de Sp. Def -2 e precisão 85; reflexão e Substitute usam o alvo refletido correto | CORRIGIDO |
| 320 | Grass Whistle | Sleep sonoro com precisão 55; Soundproof e todos os bloqueios pré-precisão seguem a ordem exata do script | CORRIGIDO |
| 321 | Tickle | Falha antes da precisão somente com Attack e Defense ambos em -6; caso contrário tenta cada queda separadamente, inclusive após Magic Coat | CORRIGIDO |
| 322 | Cosmic Power | Defense e Sp. Def +1; um stat disponível ainda sobe, ambos em +6 falham; Snatch reavalia os limites do ladrão | CORRIGIDO |
| 323 | Water Spout | Poder `max(1, 150 * HP atual / HP máximo)` calculado no instante do ataque; alvo duplo adaptado ao 1x1 | OK |
| 324 | Signal Beam | Chance de confusion após dano com Substitute, Shield Dust, Own Tempo e Safeguard | CORRIGIDO |
| 325 | Shadow Punch | Ignora precisão/evasão, mas não alcança por si só estados semi-invulneráveis | OK |

## IDs 326–354

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 326 | Extrasensory | Poder dobrado contra Minimize e 30% de flinch após dano, somente antes de o alvo agir | OK |
| 327 | Sky Uppercut | Alcança somente os estados aéreos de Fly/Bounce; demais precisão, Protect e tipo seguem o dano comum | OK |
| 328 | Sand Tomb | Aprisiona pelo contador 3–6, não reinicia uma prisão já ativa e respeita Substitute/imunidade de tipo | OK |
| 329 | Sheer Cold | OHKO falha contra alvo de nível maior ou Sturdy; precisão especial é 30 + diferença de níveis e ignora a fórmula comum | OK |
| 330 | Muddy Water | Poder 95, precisão 85 e 30% de Accuracy -1; alvo duplo adaptado ao 1x1 e Shield Dust não bloqueia a queda | CORRIGIDO |
| 331 | Bullet Seed | Multi-hit 2–5 com a distribuição Gen III e cada acerto resolvido separadamente | OK |
| 332 | Aerial Ace | Ignora precisão/evasão comum, mas não alcança por si só estados semi-invulneráveis | OK |
| 333 | Icicle Spear | Multi-hit 2–5 com a distribuição Gen III e cada acerto resolvido separadamente | OK |
| 334 | Iron Defense | Defense +2; Snatch reavalia o limite de Defense no ladrão antes de executar | CORRIGIDO |
| 335 | Block | Impede fuga/troca, falha contra Substitute/alvo já preso e usa `NO_ACC_CALC`, sem rolagem comum de precisão | CORRIGIDO |
| 336 | Howl | Attack +1; Snatch reavalia o limite de Attack no ladrão antes de executar | CORRIGIDO |
| 337 | Dragon Claw | Poder 80 e dano comum; classe física segue o split global Platinum adotado pelo projeto | OK |
| 338 | Frenzy Plant | Recarrega após acerto ou bloqueio por Protect, mas não após miss, imunidade ou absorção | OK |
| 339 | Bulk Up | Attack e Defense +1 de forma independente; falha só com ambos em +6 e Snatch reavalia ambos no ladrão | CORRIGIDO |
| 340 | Bounce | Dois turnos, estado aéreo, 30% de paralysis, bloqueio por Gravity e tabela correta de golpes que alcançam o usuário | OK |
| 341 | Mud Shot | Poder 55, precisão 95 e Speed -1 garantido após dano; Shield Dust não bloqueia a queda | CORRIGIDO |
| 342 | Poison Tail | +1 estágio crítico e 10% de poison depois do dano | OK |
| 343 | Covet | Compartilha o script de Thief: slot livre, item transferível, Substitute/Sticky Hold e restrição de roubo por NPC comum corrigidos | CORRIGIDO |
| 344 | Volt Tackle | Regra FireRed: recoil de 1/3 do dano causado e nenhum efeito adicional de paralysis | OK |
| 345 | Magical Leaf | Ignora precisão/evasão comum, mas não alcança por si só estados semi-invulneráveis | OK |
| 346 | Water Sport | Marca o usuário, reduz Fire no campo, falha só se o mesmo usuário repetir e é preservado por Baton Pass | OK |
| 347 | Calm Mind | Sp. Atk e Sp. Def +1 de forma independente; falha só com ambos em +6 e Snatch reavalia ambos | CORRIGIDO |
| 348 | Leaf Blade | Regra FireRed: poder 70 e +1 estágio crítico | OK |
| 349 | Dragon Dance | Attack e Speed +1 de forma independente; falha só com ambos em +6 e Snatch reavalia ambos | CORRIGIDO |
| 350 | Rock Blast | Multi-hit 2–5 com a distribuição Gen III e cada acerto resolvido separadamente | OK |
| 351 | Shock Wave | Ignora precisão/evasão comum, mas não alcança por si só estados semi-invulneráveis | OK |
| 352 | Water Pulse | Dano Water e 20% de confusion, respeitando Substitute, Shield Dust, Own Tempo e Safeguard | OK |
| 353 | Doom Desire | Compartilha o sistema de Future Sight: prepara sem rolar precisão, acerta após dois turnos e rola precisão/variação apenas no impacto, sem STAB/tipo/crítico | CORRIGIDO |
| 354 | Psycho Boost | Sp. Atk -2 no usuário somente se o golpe afetar o alvo; efeito `CERTAIN` sem rolagem adicional | CORRIGIDO |

## IDs 355–379

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 355 | Roost | Cura metade do HP, é bloqueado por Heal Block e remove Flying até o fim do turno; um tipo Flying puro fica sem tipo, não Normal | CORRIGIDO |
| 356 | Gravity | Campo por cinco turnos; aumenta a precisão, derruba estados aéreos e suprime Flying/Levitate/Magnet Rise para Ground, além de bloquear os golpes aéreos listados no script | OK |
| 357 | Miracle Eye | Marca o alvo para ignorar Evasion e a imunidade Dark a Psychic; repetir no mesmo alvo continua sendo sucesso | CORRIGIDO |
| 358 | Wake-Up Slap | Poder 60, dobra contra alvo dormindo e cura o sono depois de um acerto que alcance o Pokémon, não o Substitute | OK |
| 359 | Hammer Arm | Poder 100/precisão 90; reduz Speed do usuário em um estágio depois de o golpe afetar o alvo | OK |
| 360 | Gyro Ball | Poder `min(150, 1 + 25 * Speed alvo / Speed usuário)` usando o snapshot de Speed que ordenou o turno | CORRIGIDO |
| 361 | Healing Wish | Usuário desmaia e arma cura total de HP/status para o próximo aliado válido; a entrada e seus hazards ocorrem na ordem do Platinum | OK |
| 362 | Brine | Poder 65, dobrado quando o alvo está com metade ou menos do HP máximo | OK |
| 363 | Natural Gift | Exige berry ativa, usa as tabelas exatas de tipo/poder e consome a berry antes dos testes de miss/Protect; as cinco berries de sabor têm poder 60 | CORRIGIDO |
| 364 | Feint | Prioridade +2; só funciona contra Protect/Detect ativo, remove a proteção e então causa dano | OK |
| 365 | Pluck | Depois de atingir sem Substitute, remove e aplica a berry do alvo ao usuário conforme o efeito específico da berry | OK |
| 366 | Tailwind | Dobra a Speed do lado do usuário por três turnos no conjunto de regras do Platinum | OK |
| 367 | Acupressure | Escolhe uniformemente um stat que ainda possa subir e o eleva em dois; falha somente se todos os sete estiverem no limite | OK |
| 368 | Metal Burst | Devolve 1,5 vez o último dano recebido no turno, físico ou especial; falha sem dano válido | OK |
| 369 | U-turn | Causa dano e solicita a troca do usuário somente se ele sobreviver e houver substituto válido, executando a entrada normalmente | OK |
| 370 | Close Combat | Poder 120; depois de afetar o alvo reduz Defense e Sp. Def do usuário em um estágio cada | OK |
| 371 | Payback | Poder 50, dobrado quando o alvo já executou sua ação no turno, não por uma simples comparação tardia de Speed | CORRIGIDO |
| 372 | Assurance | Poder 50, dobrado se o alvo já perdeu HP no mesmo turno | CORRIGIDO |
| 373 | Embargo | Desativa o item do alvo por cinco turnos; respeita Substitute, item já ativado no turno e restaura o item ao terminar | CORRIGIDO |
| 374 | Fling | Valida item transferível, usa a tabela exata de poder, remove o item antes do acerto e só executa o efeito do item se realmente atingir | CORRIGIDO |
| 375 | Psycho Shift | Transfere o major status do usuário se o alvo puder recebê-lo e só então cura o usuário; respeita tipo, Ability, Safeguard e Substitute | OK |
| 376 | Trump Card | Não faz teste comum de precisão; poder 40/50/60/80/200 conforme o PP restante do slot do comando, inclusive quando chamado por outro golpe | CORRIGIDO |
| 377 | Heal Block | Impede por cinco turnos a lista original de golpes, itens, habilidades e efeitos de recuperação, sem bloquear dano ou efeitos não curativos | OK |
| 378 | Wring Out | Poder `max(1, 1 + 120 * HP atual / HP máximo)` calculado no momento do ataque | OK |
| 379 | Power Trick | Troca Attack e Defense efetivos do usuário até repetir ou sair de campo, sem alterar os estágios desses stats | OK |

## IDs 380–404

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 380 | Gastro Acid | Mantém a Ability bruta e aplica uma supressão separada; falha em Substitute, repetição e Multitype e interage corretamente com cópia/troca posterior | CORRIGIDO |
| 381 | Lucky Chant | Protege o lado do usuário contra críticos por cinco turnos e falha se já estiver ativo | OK |
| 382 | Me First | Prioridade normal; exige agir antes do alvo, copia o comando ofensivo escolhido naquele turno e aplica multiplicador de poder de 1,5 | CORRIGIDO |
| 383 | Copycat | Chama o último golpe global permitido pelo Platinum e falha para a lista de comandos não copiáveis ou quando não existe golpe anterior | OK |
| 384 | Power Swap | Troca os estágios de Attack e Sp. Atk entre usuário e alvo; o script não é barrado por Substitute | OK |
| 385 | Guard Swap | Troca os estágios de Defense e Sp. Def entre usuário e alvo; o script não é barrado por Substitute | OK |
| 386 | Punishment | Poder `min(200, 60 + 20 * soma dos estágios positivos do alvo)` sobre os sete stats | OK |
| 387 | Last Resort | Exige que cada outro golpe conhecido tenha sido tentado desde a entrada e não conta o próprio Last Resort como pré-requisito | OK |
| 388 | Worry Seed | Substitui a Ability bruta por Insomnia; falha contra Truant, Multitype, Griseous Orb e Substitute, preservando uma eventual supressão de Gastro Acid | CORRIGIDO |
| 389 | Sucker Punch | Prioridade +1; só acerta antes de o alvo agir quando o comando dele é ofensivo, incluindo Struggle forçado | CORRIGIDO |
| 390 | Toxic Spikes | Máximo de duas camadas; envenena entradas aterradas, Poison absorve, e Safeguard, Gravity, Flying, Levitate, Magnet Rise e Magic Guard seguem o subscript original | CORRIGIDO |
| 391 | Heart Swap | Troca todos os sete estágios e também Focus Energy/critical stage; atravessa Substitute como no script | CORRIGIDO |
| 392 | Aqua Ring | Marca o usuário e cura 1/16 no fim de cada turno enquanto não houver Heal Block | OK |
| 393 | Magnet Rise | Dá imunidade a Ground por cinco turnos; falha sob Gravity, com Levitate, Ingrain ou Magnet Rise já ativo | OK |
| 394 | Flare Blitz | Poder 120, recoil de 1/3 do dano real, 10% de burn e possibilidade de ser usado para descongelar o próprio usuário | OK |
| 395 | Force Palm | Poder 60 e 30% de paralysis após dano, respeitando todas as imunidades e proteções do efeito adicional | OK |
| 396 | Aura Sphere | Poder 90 e acerto sem cálculo comum de Accuracy/Evasion, mantendo Protect, imunidade e estados inalcançáveis | OK |
| 397 | Rock Polish | Speed +2 no usuário, com falha no limite e compatibilidade com Snatch | OK |
| 398 | Poison Jab | Poder 80 e 30% de poison depois do dano | OK |
| 399 | Dark Pulse | Poder 80 e 20% de flinch somente se o alvo ainda puder agir | OK |
| 400 | Night Slash | Poder 70 e +1 estágio de crítico | OK |
| 401 | Aqua Tail | Poder 90, precisão 90 e dano Water físico comum | OK |
| 402 | Seed Bomb | Poder 80 e dano Grass físico comum, sem efeito adicional | OK |
| 403 | Air Slash | Poder 75, precisão 95 e 30% de flinch somente antes da ação do alvo | OK |
| 404 | X-Scissor | Poder 80 e dano Bug físico comum, sem efeito adicional | OK |

## IDs 405–429

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 405 | Bug Buzz | Golpe sonoro de poder 90 com 10% de Sp. Def -1; Soundproof e o efeito secundário usam os caminhos originais | OK |
| 406 | Dragon Pulse | Poder 90 e dano Dragon especial comum | OK |
| 407 | Dragon Rush | Poder 100, precisão 75 e 20% de flinch depois do dano | OK |
| 408 | Power Gem | Poder 70 e dano Rock especial comum | OK |
| 409 | Drain Punch | Cura metade do HP realmente removido, não drena Substitute e Liquid Ooze inverte a recuperação | OK |
| 410 | Vacuum Wave | Poder 40 e prioridade +1, usando dano Fighting especial | OK |
| 411 | Focus Blast | Poder 120, precisão 70 e 10% de Sp. Def -1 depois do dano | OK |
| 412 | Energy Ball | Poder 80 e 10% de Sp. Def -1 depois do dano | OK |
| 413 | Brave Bird | Poder 120 e recoil de 1/3 do dano real; Rock Head suprime o recoil | OK |
| 414 | Earth Power | Poder 90 e 10% de Sp. Def -1 depois do dano | OK |
| 415 | Switcheroo | Troca itens segundo o mesmo núcleo de Trick; Substitute, Sticky Hold, Multitype, Griseous Orb, Klutz/Embargo e itens não transferíveis seguem o script | CORRIGIDO |
| 416 | Giga Impact | Poder 150; recarrega após acerto ou Protect/Detect, mas não após miss real, imunidade ou absorção | CORRIGIDO |
| 417 | Nasty Plot | Sp. Atk +2 no usuário, com falha no limite e compatibilidade com Snatch | OK |
| 418 | Bullet Punch | Poder 40 e prioridade +1 | OK |
| 419 | Avalanche | Prioridade -4 e poder 60, dobrado se o usuário já recebeu dano no turno | OK |
| 420 | Ice Shard | Poder 40 e prioridade +1 | OK |
| 421 | Shadow Claw | Poder 70 e +1 estágio de crítico | OK |
| 422 | Thunder Fang | Poder 65/precisão 95; paralysis e flinch têm rolagens independentes de 10% na ordem do subscript | CORRIGIDO |
| 423 | Ice Fang | Poder 65/precisão 95; freeze e flinch têm rolagens independentes de 10% na ordem do subscript | CORRIGIDO |
| 424 | Fire Fang | Poder 65/precisão 95; burn e flinch independentes e preservação do bug retail que atravessa Wonder Guard em eficácia neutra | CORRIGIDO |
| 425 | Shadow Sneak | Poder 40 e prioridade +1 | OK |
| 426 | Mud Bomb | Poder 65, precisão 85 e 30% de Accuracy -1 depois do dano | OK |
| 427 | Psycho Cut | Poder 70 e +1 estágio de crítico, sem contato | OK |
| 428 | Zen Headbutt | Poder 80, precisão 90 e 20% de flinch | OK |
| 429 | Mirror Shot | Poder 65, precisão 85 e 30% de Accuracy -1 depois do dano | OK |

## IDs 430–454

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 430 | Flash Cannon | Poder 80 e 10% de Sp. Def -1 depois do dano | OK |
| 431 | Rock Climb | Poder 90, precisão 85 e 20% de confusion depois do dano | OK |
| 432 | Defog | Tenta Evasion -1 e depois remove apenas do lado do alvo Reflect, Light Screen, Safeguard, Mist, Spikes, Toxic Spikes e Stealth Rock | CORRIGIDO |
| 433 | Trick Room | Prioridade -7; cria ou encerra o campo de cinco turnos que inverte somente a ordem por Speed | OK |
| 434 | Draco Meteor | Poder 140/precisão 90; Sp. Atk -2 no usuário por efeito certo somente quando o golpe afeta o alvo | OK |
| 435 | Discharge | Poder 80 e 30% de paralysis; alvo amplo do cartucho adaptado ao oponente único do 1x1 | OK |
| 436 | Lava Plume | Poder 80 e 30% de burn; alvo amplo do cartucho adaptado ao oponente único do 1x1 | OK |
| 437 | Leaf Storm | Poder 140/precisão 90; Sp. Atk -2 no usuário por efeito certo somente quando o golpe afeta o alvo | OK |
| 438 | Power Whip | Poder 120, precisão 85 e dano Grass físico comum | OK |
| 439 | Rock Wrecker | Poder 150; recarga segue as mesmas regras exatas de Hyper Beam/Giga Impact | CORRIGIDO |
| 440 | Cross Poison | Poder 70, +1 estágio crítico e 10% de poison depois do dano | OK |
| 441 | Gunk Shot | Poder 120, precisão 70 e 30% de poison depois do dano | OK |
| 442 | Iron Head | Poder 80 e 30% de flinch somente se o alvo ainda puder agir | OK |
| 443 | Magnet Bomb | Poder 60 e acerto sem cálculo comum de Accuracy/Evasion | OK |
| 444 | Stone Edge | Poder 100, precisão 80 e +1 estágio de crítico | OK |
| 445 | Captivate | Sp. Atk -2 somente em alvo de gênero oposto; falha para gênero inválido/igual ou Oblivious e respeita Substitute na etapa de stat | OK |
| 446 | Stealth Rock | Uma camada; na entrada causa `HP máximo * eficácia de Rock / 8`, com a saída antecipada de Magic Guard do subscript de hazards | CORRIGIDO |
| 447 | Grass Knot | Poder 20/40/60/80/100/120 pelas seis faixas de peso do alvo | OK |
| 448 | Chatter | Poder 60; confusion só por Chatot não transformado e com alvo vivo; sem microfone, o parâmetro de voz foi fixado para produzir 31% no comparador do projeto | ADAPTADO |
| 449 | Judgment | Poder 100 e tipo definido pela Plate ativa do usuário, sem consumir o item | OK |
| 450 | Bug Bite | Depois de atingir sem Substitute, remove e aplica ao usuário a berry do alvo | OK |
| 451 | Charge Beam | Poder 50/precisão 90 e 70% de Sp. Atk +1 no usuário; o autoefeito não é bloqueado pelo Substitute do alvo | CORRIGIDO |
| 452 | Wood Hammer | Poder 120 e recoil de 1/3 do dano real; Rock Head suprime o recoil | OK |
| 453 | Aqua Jet | Poder 40 e prioridade +1 | OK |
| 454 | Attack Order | Poder 90 e +1 estágio de crítico | OK |

## IDs 455–467

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 455 | Defend Order | Defense e Sp. Def +1 de forma independente; falha apenas se ambos já estiverem no limite | OK |
| 456 | Heal Order | Cura metade do HP máximo, falha em HP cheio e é bloqueado por Heal Block | OK |
| 457 | Head Smash | Poder 150/precisão 80 e recoil de metade do dano real; Rock Head suprime o recoil | OK |
| 458 | Double Hit | Exatamente dois acertos de poder 35 depois de uma única validação inicial de precisão | OK |
| 459 | Roar of Time | Poder 150; recarga após acerto ou Protect/Detect e não após miss real, imunidade ou absorção | CORRIGIDO |
| 460 | Spacial Rend | Poder 100, precisão 95 e +1 estágio de crítico | OK |
| 461 | Lunar Dance | Usuário desmaia e o próximo aliado válido recebe HP, status e PP completos depois da entrada | OK |
| 462 | Crush Grip | Versão física de Wring Out: poder `max(1, 1 + 120 * HP atual / HP máximo)` | OK |
| 463 | Magma Storm | Poder 120/precisão 70; prende pelo contador original e aplica o residual de aprisionamento enquanto ativo | OK |
| 464 | Dark Void | Sleep primário com precisão 80; o alvo amplo do cartucho é adaptado ao oponente único no 1x1 | OK |
| 465 | Seed Flare | Poder 120, precisão 85 e 40% de Sp. Def -2 depois do dano | OK |
| 466 | Ominous Wind | Poder 60 e 10% de elevar Attack, Defense, Sp. Atk, Sp. Def e Speed do usuário | OK |
| 467 | Shadow Force | Dois turnos, estado semi-invulnerável e golpe de poder 120 que ignora e remove Protect/Detect no ataque | OK |

### Contrato de precisão 101 da Gen 5

Black/White usa `101` nos dados para representar precisão `--`, e não uma chance literal de 101%. Os 22 golpes afetados (468, 469, 470, 471, 472, 475, 476, 477, 478, 483, 489, 495, 499, 501, 502, 504, 505, 508, 513, 516, 526 e 538) agora ignoram somente a rolagem comum de Accuracy/Evasion, preservando alcance, imunidades e condições próprias. A regressão executa todos dos dois lados com Accuracy -6 contra Evasion +6 e confirma 44/44 usos sem `Missed`; os casos deliberadamente sem alvo útil no 1x1 continuam seguindo sua regra própria.

## IDs 468–489 — novas moves de Unova

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 468 | Hone Claws | Attack +1 e Accuracy +1 no usuário, sem falha falsa enquanto ao menos um stat puder subir | OK |
| 469 | Wide Guard | Proteção de lado contra golpes amplos, com prioridade e contador próprios | OK |
| 470 | Guard Split | Defense e Sp. Def de usuário e alvo passam para as médias correspondentes | OK |
| 471 | Power Split | Attack e Sp. Atk de usuário e alvo passam para as médias correspondentes | OK |
| 472 | Wonder Room | Ativa ou encerra a troca global entre Defense e Sp. Def; ambos os eventos possuem apresentação própria e não caem mais no texto falso de falha | CORRIGIDO |
| 473 | Psyshock | Dano Psychic especial calculado contra a Defense física do alvo | OK |
| 474 | Venoshock | Poder dobra quando o alvo está poisoned ou badly poisoned | OK |
| 475 | Autotomize | Speed +2 e redução persistente do peso de batalha | OK |
| 476 | Rage Powder | Redireciona ataques de parceiros em batalha de duplas; não possui alvo útil no 1x1 | EXCLUÍDO DO 1x1 |
| 477 | Telekinesis | Aplica o estado de levitação/acerto garantido pelo período próprio, respeitando alvos inválidos | OK |
| 478 | Magic Room | Ativa ou encerra a supressão global dos efeitos de held items | OK |
| 479 | Smack Down | Causa dano e aterra o alvo, removendo a imunidade aérea aplicável | OK |
| 480 | Storm Throw | Dano Fighting com crítico garantido | OK |
| 481 | Flame Burst | Dano Fire principal; o dano colateral de parceiros não existe no 1x1 | ADAPTADO AO 1x1 |
| 482 | Sludge Wave | Dano Poison e chance de poison, usando o único alvo adversário do 1x1 | OK |
| 483 | Quiver Dance | Sp. Atk, Sp. Def e Speed +1 no usuário | OK |
| 484 | Heavy Slam | Poder escolhido pela razão de peso entre usuário e alvo | OK |
| 485 | Synchronoise | Só atinge alvo que compartilha ao menos um tipo com o usuário | OK |
| 486 | Electro Ball | Poder escolhido pela razão de Speed entre usuário e alvo | OK |
| 487 | Soak | Converte o alvo para Water, respeitando bloqueios de mudança de tipo | OK |
| 488 | Flame Charge | Dano Fire e Speed +1 certo no usuário após o acerto | OK |
| 489 | Coil | Attack, Defense e Accuracy +1 no usuário | OK |

## IDs 490–511 — novas moves de Unova

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 490 | Low Sweep | Dano Fighting e Speed -1 no alvo | OK |
| 491 | Acid Spray | Dano Poison e Sp. Def -2 no alvo | OK |
| 492 | Foul Play | Usa o Attack do alvo no cálculo do dano físico | OK |
| 493 | Simple Beam | Troca a habilidade do alvo por Simple quando a habilidade permite alteração | OK |
| 494 | Entrainment | Copia a habilidade do usuário para o alvo quando ambas permitem alteração | OK |
| 495 | After You | Não falha antes da ação do alvo; registra o ajuste de ordem e só falha se o alvo já agiu | CORRIGIDO |
| 496 | Round | Dano sonoro normal; o bônus cooperativo de parceiro não se aplica ao 1x1 | ADAPTADO AO 1x1 |
| 497 | Echoed Voice | Aumenta de poder em usos consecutivos e reinicia a sequência quando ela é quebrada | OK |
| 498 | Chip Away | Ignora mudanças defensivas relevantes do alvo no cálculo de dano | OK |
| 499 | Clear Smog | Causa dano e restaura os estágios de stats do alvo ao neutro | OK |
| 500 | Stored Power | Poder cresce por cada estágio positivo do usuário | OK |
| 501 | Quick Guard | Protege o lado contra golpes de prioridade, com prioridade e contador próprios | OK |
| 502 | Ally Switch | Troca a posição com um parceiro em batalha de duplas; não existe parceiro no 1x1 | EXCLUÍDO DO 1x1 |
| 503 | Scald | Dano Water e 30% de burn | OK |
| 504 | Shell Smash | Defense/Sp. Def -1 e Attack/Sp. Atk/Speed +2, aplicados como um único efeito | OK |
| 505 | Heal Pulse | Cura metade do HP máximo do alvo; como no 1x1 o único alvo seria o oponente, permanece no catálogo canônico mas não pode ocupar moveset local | EXCLUÍDO DO 1x1 |
| 506 | Hex | Poder dobra quando o alvo possui status principal | OK |
| 507 | Sky Drop | Primeiro turno carrega alvo e usuário; segundo turno libera e causa dano, com restrições próprias | OK — 2 TURNOS |
| 508 | Shift Gear | Attack +1 e Speed +2 no usuário | OK |
| 509 | Circle Throw | Causa dano mesmo sem reserva disponível; a troca forçada só ocorre quando há substituto válido | CORRIGIDO |
| 510 | Incinerate | Causa dano e destrói berry/gem transferível do alvo após atingir | OK |
| 511 | Quash | Não falha antes da ação do alvo; registra a postergação e só falha se o alvo já agiu | CORRIGIDO |

## IDs 512–533 — novas moves de Unova

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 512 | Acrobatics | Poder dobra quando o usuário não possui held item ativo | OK |
| 513 | Reflect Type | Copia os tipos atuais do alvo para o usuário | OK |
| 514 | Retaliate | Poder dobra quando um aliado do usuário desmaiou no turno anterior | OK |
| 515 | Final Gambit | Remove do alvo HP igual ao HP atual do usuário e então desmaia o usuário | OK |
| 516 | Bestow | Entrega o held item do usuário ao alvo vazio, respeitando itens não transferíveis | OK |
| 517 | Inferno | Precisão 50, dano Fire e burn garantido após o acerto | OK |
| 518 | Water Pledge | Dano Water normal; combinações entre parceiros não se aplicam ao 1x1 | ADAPTADO AO 1x1 |
| 519 | Fire Pledge | Dano Fire normal; combinações entre parceiros não se aplicam ao 1x1 | ADAPTADO AO 1x1 |
| 520 | Grass Pledge | Dano Grass normal; combinações entre parceiros não se aplicam ao 1x1 | ADAPTADO AO 1x1 |
| 521 | Volt Switch | Causa dano e retorna o usuário quando existe reserva válida | OK |
| 522 | Struggle Bug | Dano Bug e Sp. Atk -1 no alvo | OK |
| 523 | Bulldoze | Dano Ground e Speed -1 no alvo | OK |
| 524 | Frost Breath | Dano Ice com crítico garantido | OK |
| 525 | Dragon Tail | Causa dano mesmo sem reserva disponível; a troca forçada só ocorre quando há substituto válido | CORRIGIDO |
| 526 | Work Up | Attack +1 e Sp. Atk +1 no usuário | OK |
| 527 | Electroweb | Dano Electric e Speed -1 no alvo | OK |
| 528 | Wild Charge | Dano Electric e recoil proporcional ao dano realmente causado | OK |
| 529 | Drill Run | Dano Ground com estágio crítico elevado | OK |
| 530 | Dual Chop | Exatamente dois acertos Dragon depois da validação inicial | OK |
| 531 | Heart Stamp | Dano Psychic e chance de flinch somente antes da ação do alvo | OK |
| 532 | Horn Leech | Cura metade do HP realmente removido do alvo | OK |
| 533 | Sacred Sword | Ignora mudanças de Defense/Evasion relevantes do alvo no cálculo | OK |

## IDs 534–559 — novas moves de Unova

| ID | Golpe | Fonte e comportamento conferidos | Resultado |
|---:|---|---|---|
| 534 | Razor Shell | Dano Water e chance de Defense -1 | OK |
| 535 | Heat Crash | Poder escolhido pela razão de peso entre usuário e alvo | OK |
| 536 | Leaf Tornado | Dano Grass e chance de Accuracy -1 | OK |
| 537 | Steamroller | Dano Bug, chance de flinch e interação de dano dobrado com alvo encolhido | OK |
| 538 | Cotton Guard | Defense +3 no usuário | OK |
| 539 | Night Daze | Dano Dark e chance de Accuracy -1 | OK |
| 540 | Psystrike | Dano Psychic especial calculado contra a Defense física do alvo | OK |
| 541 | Tail Slap | Dano Normal de dois a cinco acertos | OK |
| 542 | Hurricane | Dano Flying, chance de confusion e regras climáticas de precisão | OK |
| 543 | Head Charge | Dano Normal e recoil proporcional ao dano realmente causado | OK |
| 544 | Gear Grind | Exatamente dois acertos Steel depois da validação inicial | OK |
| 545 | Searing Shot | Dano Fire e chance de burn, adaptado ao único oponente do 1x1 | OK |
| 546 | Techno Blast | Tipo determinado pelo Drive ativo do usuário | OK |
| 547 | Relic Song | Dano sonoro, chance de sleep e alternância da forma de Meloetta quando aplicável | OK |
| 548 | Secret Sword | Dano Fighting especial calculado contra a Defense física do alvo | OK |
| 549 | Glaciate | Dano Ice e Speed -1 no alvo | OK |
| 550 | Bolt Strike | Dano Electric e chance de paralysis | OK |
| 551 | Blue Flare | Dano Fire e chance de burn | OK |
| 552 | Fiery Dance | Dano Fire e chance de Sp. Atk +1 no usuário | OK |
| 553 | Freeze Shock | Carrega no primeiro turno; no segundo causa dano Ice e pode paralisar | OK — 2 TURNOS |
| 554 | Ice Burn | Carrega no primeiro turno; no segundo causa dano Ice e pode queimar | OK — 2 TURNOS |
| 555 | Snarl | Dano Dark e Sp. Atk -1 no alvo | OK |
| 556 | Icicle Crash | Dano Ice e chance de flinch somente antes da ação do alvo | OK |
| 557 | V-create | Dano Fire e reduz Defense, Sp. Def e Speed do usuário depois do acerto | OK |
| 558 | Fusion Flare | Dano Fire e bônus quando usado após Fusion Bolt no mesmo turno | OK |
| 559 | Fusion Bolt | Dano Electric e bônus quando usado após Fusion Flare no mesmo turno | OK |
