# Atualização Bluetooth

## Arquivos

- Aplicativo: `app/PokegochiUpdater-v2.apk`
- Pacote assinado: `ota/pokegochi-v2.pgota`

## Procedimento

1. Instale o APK no Android. Pode ser necessário permitir instalação dessa
   fonte nas configurações do aparelho.
2. Copie o arquivo `.pgota` para Downloads no celular.
3. No Pokegochi, abra Settings e toque em `UPDATE`.
4. A tela mostra o código de pareamento e inicia o advertising BLE.
5. No aplicativo, selecione o pacote, encontre o dispositivo, informe o código
   e confirme.
6. Mantenha celular e Pokegochi próximos e alimentados até a verificação final.

O pacote atualiza o slot inativo do firmware e o arquivo de assets do microSD.
Cada parte é verificada por SHA-256 e a assinatura P-256 é validada na placa.
Uma transferência interrompida pode ser retomada; a versão anterior permanece
disponível para rollback enquanto a atualização não for confirmada.

O save não faz parte do pacote e não é substituído.
