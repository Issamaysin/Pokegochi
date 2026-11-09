# Pokegochi Online Server

Servidor doméstico para até doze jogadores e seis salas simultâneas. A ESP32
mantém presença, abre salas e encaminha os pacotes. Batalhas entre jogadores
continuam nos clientes; além disso, o servidor hospeda o jogador virtual
reservado `99999`.

## Jogador virtual 99999

- Aparece uma única vez e sempre no fim do lobby quando está livre.
- Aceita qualquer convite de batalha e recusa toda tentativa de troca.
- Desaparece da descoberta enquanto está em uma sessão e reaparece ao liberar
  a sala, inclusive após queda ou cancelamento do adversário.
- Possui uma Box fixa de 70 Pokémon competitivos: dez de cada geração I–VII,
  pelo menos um lendário por geração e um shiny no total.
- Dragonite, Wobbuffet, Latios, Rayquaza, Lucario, Darkrai, Greninja com
  Ash's Bond, Aegislash, Xerneas, Zygarde e Mimikyu fazem parte permanente da
  Box; não dependem do sorteio determinístico de preenchimento.
- Sorteia três membros distintos por partida. Se houver Mega elegível, sorteia
  um deles para a Mega Stone; outro membro compatível recebe um Z-Crystal; os
  demais mantêm held items próprios. Ash's Bond é travado no Greninja e nunca
  é substituído por uma dessas duas decorações de equipe.
- Usa nível 70 em `BALANCED` e nível 100 em `NORMAL`.
- O contador de vitórias `BALANCED` é persistido em NVS e enviado no mesmo
  campo de lobby usado pelos jogadores reais.

O Player ID alto torna o cliente humano a autoridade determinística da batalha.
Assim, a ESP32 envia roster e decisões do bot, enquanto o motor completo e o
snapshot canônico continuam no cliente, sem duplicar o motor do jogo no
servidor.

## Configuração fixa do DevKit

1. Copie `include/ServerLocalConfig.example.h` para
   `include/ServerLocalConfig.h` e preencha a rede 2,4 GHz da casa. Esse arquivo
   é privado e ignorado pelo Git.
2. Grave o ambiente `pokegochi-online-server` desta pasta.
3. Não é necessário abrir porta no roteador: a ESP32 conecta por saída ao
   relay MQTT definido em `include/config/OnlineRelayConfig.h`.
4. O TCP `32145` permanece disponível automaticamente como rota rápida para os
   jogadores que estiverem na mesma LAN.

O jogador não informa IP, porta nem código. Na mesma rede, o servidor anuncia
automaticamente `_pokegochi._tcp` por DNS-SD/mDNS como
`pokegochi-server.local`. Fora dela, Android, Pokegochi físico e servidor se
encontram pelo relay MQTT; isso inclui o celular em 3G/4G/5G e redes atrás de
CGNAT. O Android usa a conexão ativa sem setup; somente o Pokegochi físico
escaneia SSIDs e pede a senha da rede.

O comando serial `STATUS` mostra a saúde do servidor, jogadores, salas, estado
do bot e vitórias `BALANCED`. Depois de uma queda, ele reconecta sozinho;
partidas interrompidas são descartadas e os clientes retornam ao lobby.
