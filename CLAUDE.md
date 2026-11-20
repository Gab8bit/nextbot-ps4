# Progetto: Nextbot per PS4 (homebrew, OpenOrbis Toolchain)

## Obiettivo
Sviluppa in **C/C++** un gioco homebrew per PS4 in stile "nextbot": un'immagine PNG (il bot) con un audio associato ti insegue dentro una mappa e devi scappare. L'utente carica i propri PNG e audio in cartelle esterne, e dal menu iniziale sceglie quale bot usare.

## Ambiente
- Toolchain: **OpenOrbis PS4 Toolchain** (variabile `OO_PS4_TOOLCHAIN`), con `create-fself` / `create-gp4` / `PkgTool.Core` per generare il PKG.
- Console con exploit/mod (GoldHEN) che esegue homebrew.
- **Non usare** SDK Sony ufficiale, SDL, raylib o librerie non portate su OpenOrbis.
- Prima di scrivere codice, **leggi gli header e i sample di OpenOrbis** (`$OO_PS4_TOOLCHAIN/include`, cartella `samples`) e usa solo API che esistono davvero. **Non inventare funzioni.** Se non sei sicuro di una firma, controllala negli header.

## Requisiti funzionali

### 1. Asset esterni
- Cartelle: `/data/nextbot/png/` e `/data/nextbot/sounds/` (`/app0/` è read-only, quindi gli asset NON vanno nel PKG).
- All'avvio scansiona `png/` e abbina `nome.png` a `nome.wav` (stesso nome base, estensione diversa).
- Se manca l'audio, il bot è comunque selezionabile ma silenzioso. Se non c'è nessun PNG, mostra a schermo un messaggio chiaro con il percorso da usare.
- Se le cartelle non esistono, crearle automaticamente.

### 2. Menu di selezione
- Lista dei bot trovati (nome file + anteprima piccola dello sprite).
- Navigazione con D-pad su/giù, **X** per confermare, **cerchio** per tornare indietro.
- Opzionale: scelta difficoltà (velocità del bot: lenta / media / veloce).
- Il menu principale ha tre voci: **Gioca**, **Classifica**, **Impostazioni**.

### 2b. Impostazioni
- Voce **"Resetta classifica"**: chiede **conferma** esplicita ("Sei sicuro? Questa azione non si può annullare") con scelta Sì/No (default su **No**), poi cancella tutti i record e il file di salvataggio.
- Dopo il reset mostra un messaggio di conferma a schermo.
- Struttura le impostazioni come lista di voci, così in futuro se ne possono aggiungere altre (volume generale, difficoltà predefinita, ecc.).

### 3. Gameplay
- Mappa a **griglia di tile** (0 = libero, 1 = muro), definita come array statico nel codice, con la struttura pronta per caricare più mappe in futuro.
- Giocatore mosso con **stick sinistro** (analogico), con collisione contro i muri.
- Bot che insegue il giocatore con **pathfinding A\*** (o BFS) sulla griglia, ricalcolando il percorso a intervalli (non a ogni frame).
- Il bot parte lontano dal giocatore e accelera leggermente col tempo.
- **Audio spaziale semplice**: il volume del bot è funzione della distanza (più vicino = più forte), con volume minimo > 0 per sentirlo sempre.
- **Game over** quando la distanza tra bot e giocatore scende sotto il raggio di collisione: schermata con tempo sopravvissuto, poi X per tornare al menu.
- Un timer di sopravvivenza visibile a schermo.

### 4. Classifica (unico salvataggio del gioco)
- Per ogni **PNG/bot** si salva il **miglior tempo di sopravvivenza**. Formato del record: `nomeutente`, `nome-png`, `tempo`. Esempio mostrato a schermo: `Mario | shrek | 2 min 6 sec`.
- **Nome utente**: prendilo dall'utente PS4 loggato tramite le API `sceUserService` (verifica negli header/sample di OpenOrbis come ottenere lo user id e il nome). Se non riesci a ottenerlo, usa il fallback `Player`.
- A fine partita (game over): se il tempo è **migliore** del record esistente per quel PNG, aggiorna il record e mostra "NUOVO RECORD!". Se il PNG non ha ancora un record, crealo.
- La schermata **Classifica** mostra tutti i record, uno per riga, ordinati per nome PNG o per tempo (a tua scelta, ma coerente), con tempo formattato `X min Y sec` (sotto il minuto: `Y sec`).
- **Persistenza**: file `/data/nextbot/leaderboard.dat` (formato semplice e robusto: testo CSV oppure binario con magic number + versione + numero record). Gestisci file mancante, corrotto o vuoto senza crash (in tal caso parti con classifica vuota).
- **Scrittura sicura**: scrivi su file temporaneo e poi rinomina, per non corrompere i dati se la console si spegne a metà scrittura.
- Limita la lunghezza di nome utente e nome PNG (buffer sicuri, niente overflow) e sanifica i caratteri che romperebbero il formato (es. virgole nel CSV).
- Se un PNG viene rimosso dalla cartella, il suo record resta salvato (compare comunque in classifica).
- **Reset**: funzione `leaderboard_reset()` che svuota la lista in memoria ed elimina/riscrive il file, richiamata dalle Impostazioni (vedi sezione 2b).

## Requisiti tecnici
- **Video**: `libSceVideoOut` con framebuffer (1920x1080, double buffering) e **software rendering** (blit di sprite, rettangoli per tile, testo con font bitmap semplice incluso nel codice).
- **PNG**: `stb_image.h` (header-only). Ridimensiona gli sprite **una sola volta al caricamento** (non a ogni frame).
- **Audio**: `libSceAudioOut`, PCM 16-bit. Supporta **WAV** (parser minimale del header RIFF, gestisci mono/stereo e sample rate comuni). Il playback in **loop** va in un **thread separato** dal loop grafico, per evitare glitch.
- **Input**: `libScePad` (stick, D-pad, pulsanti). Inizializza correttamente utente e pad.
- **Memoria**: allocazioni contenute, libera tutto in uscita, gestisci i fallimenti di `fopen`/`malloc` senza crash.
- Loop di gioco a **60 fps** con delta time.

## Architettura del codice
Separa bene i livelli, così il codice è pulito e testabile:

```
src/
  main.cpp          // init sistema, loop principale, state machine
  platform/
    video.cpp/.h    // framebuffer, flip, vsync
    audio.cpp/.h    // thread audio, mixing, volume
    input.cpp/.h    // pad
    fs.cpp/.h       // scansione cartelle, lettura/scrittura file
    user.cpp/.h     // nome utente PS4 (sceUserService)
  game/
    map.cpp/.h      // griglia, collisioni
    pathfind.cpp/.h // A*
    player.cpp/.h
    bot.cpp/.h
    menu.cpp/.h
    leaderboard.cpp/.h // record, salvataggio, reset
    settings.cpp/.h    // schermata impostazioni + conferma reset
  gfx/
    sprite.cpp/.h   // caricamento PNG, resize, blit con alpha
    font.cpp/.h     // font bitmap
third_party/
  stb_image.h
Makefile
sce_sys/            // param.sfo, icon0.png, ecc.
```

State machine: `MENU -> PLAYING -> GAME_OVER -> MENU`, più `MENU -> LEADERBOARD -> MENU` e `MENU -> SETTINGS -> MENU`.

## Build e packaging
- Scrivi un **Makefile** compatibile con OpenOrbis (parti da quello dei sample del toolchain).
- Il processo deve produrre l'`eboot.bin` (via `create-fself`) e il **PKG** finale (via `create-gp4` + `PkgTool.Core`).
- Includi i file `sce_sys` necessari (param.sfo con titolo, TITLE_ID tipo `NBOT00001`, e `icon0.png` segnaposto).
- Spiega nel README come compilare, come generare il PKG e dove copiare le cartelle `png/` e `sounds/` sulla console (USB o FTP).

## Modo di lavorare (importante)
1. **Prima esplora** il toolchain installato e i sample, poi proponi un piano breve.
2. Procedi **a step incrementali** e fai compilare ogni step prima di passare al successivo:
   1. Skeleton che compila e mostra uno schermo colorato.
   2. Input pad funzionante (stampa/mostra lo stato dei tasti).
   3. Caricamento PNG e blit sprite.
   4. Audio WAV in loop su thread separato.
   5. Scansione cartelle + menu.
   6. Mappa, giocatore, collisioni.
   7. Bot con A\* + volume per distanza.
   8. Game over, timer.
   9. Classifica: salvataggio/caricamento file, nome utente PS4, schermata Classifica.
   10. Impostazioni con reset classifica (con conferma).
   11. Rifiniture.
3. Se un'API OpenOrbis non è chiara o manca, **dimmelo** invece di improvvisare, e proponi un'alternativa.
4. Commenta il codice dove la logica non è ovvia, ma senza esagerare.
5. A ogni step indicami come testare (log, schermo, ecc.), tenendo conto che non posso fare debug interattivo sulla console.

## Fuori scope
- Niente rete, niente 3D. L'**unico** salvataggio è la classifica (nessun altro dato persistente).
- Niente supporto OGG/MP3 nella prima versione (eventualmente `stb_vorbis` come estensione futura).
