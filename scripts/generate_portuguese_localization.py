"""Build the complete EN -> PT-BR runtime text catalogue.

The checked-in .inc is consumed directly by the firmware and Android builds;
normal builds never require network access. Re-running this tool reuses every
translation already present in the generated file and uses the local EN-PT
model only for newly discovered player-visible strings.
"""
from __future__ import annotations

import ast
import json
import re
import runpy
import sys
import unicodedata
import urllib.request
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src/game/PortugueseLocalizationGenerated.inc"
CACHE = ROOT / ".artifacts/portuguese-localization-cache.json"
LOCAL_RUNTIME = ROOT / ".artifacts/ct2-runtime"
LOCAL_MODEL = ROOT / ".artifacts/opus-mt-en-ptbr-ct2"
LOCAL_MODEL_BASE = (
    "https://huggingface.co/gow-solucoes/opus-mt-en-pt-ct2/resolve/main/"
)
CATALOG_ENGINE_VERSION = "ptbr-pob-ct2-v3"


MANUAL = {
    "4.1 WIFI": "4.1 WI-FI",
    "APP 3.1 RELEASE": "VERSAO DO APP 3.1",
    "EN": "EN",
    "PT": "PT",
    "ENGLISH": "INGLES",
    "PORTUGUESE": "PORTUGUES",
    "PORTUGUESE (BRAZIL)": "PORTUGUES BRASIL",
    "LANGUAGE": "IDIOMA",
    "SETTINGS": "CONFIGURACOES",
    "BACK": "VOLTAR",
    "NEXT": "PROXIMO",
    "PREV": "ANTERIOR",
    "MORE": "MAIS",
    "TOP": "INICIO",
    "DONE": "PRONTO",
    "OPEN": "ABRIR",
    "SAVE": "SALVAR",
    "SAVED": "SALVO",
    "LOAD": "CARREGAR",
    "CHOOSE": "ESCOLHER",
    "SELECT": "SELECIONAR",
    "SELECTED": "SELECIONADO",
    "CANCEL": "CANCELAR",
    "CONTINUE": "CONTINUAR",
    "YES": "SIM",
    "NO": "NAO",
    "ON": "LIGADO",
    "OFF": "DESLIGADO",
    "READY": "PRONTO",
    "ARRANGE PARTY, THEN PRESS READY.":
        "ARRUME A EQUIPE E APERTE PRONTO.",
    "OTHER PLAYER READY - BALANCED":
        "OUTRO JOGADOR PRONTO - EQUILIBRADO",
    "OTHER PLAYER READY - NORMAL":
        "OUTRO JOGADOR PRONTO - NORMAL",
    "OTHER PLAYER READY. PREPARE YOUR PARTY.":
        "OUTRO JOGADOR PRONTO. PREPARE SUA EQUIPE.",
    "PARTY LOCKED. WAITING FOR PLAYER...":
        "EQUIPE TRAVADA. AGUARDANDO JOGADOR...",
    "SENDING PARTY TO PLAYER...":
        "ENVIANDO EQUIPE AO JOGADOR...",
    "YOUR PARTY IS LOCKED.": "SUA EQUIPE ESTA TRAVADA.",
    "WAITING": "AGUARDANDO",
    "PLEASE": "POR FAVOR",
    "PLEASE WAIT": "AGUARDE",
    "PLEASE WAIT...": "AGUARDE...",
    "PREPARING": "PREPARANDO",
    "PREPARING BLUETOOTH": "PREPARANDO BLUETOOTH",
    "PRESENTED": "EXIBIDO",
    "STARTING BLUETOOTH...": "LIGANDO BLUETOOTH...",
    "STARTING WIFI...": "LIGANDO WI-FI...",
    "STOPPING": "DESLIGANDO",
    "STOPPING BLUETOOTH...": "DESLIGANDO BLUETOOTH...",
    "STOPPING WIFI...": "DESLIGANDO WI-FI...",
    "CONNECTING TO WIFI": "CONECTANDO AO WI-FI",
    "CLOSING BLUETOOTH": "DESLIGANDO BLUETOOTH",
    "CLOSING WIFI": "DESLIGANDO WI-FI",
    "WIFI MEMORY COULD NOT START.": "NAO FOI POSSIVEL INICIAR A MEMORIA DO WI-FI.",
    # Short fragments are collected from fitted ternary labels; retain their
    # literal spelling instead of asking the sentence model to reinterpret
    # them as unrelated words.
    "FI": "FI",
    "WI": "WI",
    "RESTORING DISPLAY...": "RESTAURANDO A TELA...",
    "FAILED": "FALHOU",
    "FAIL": "FALHA",
    "OK": "OK",
    "EMPTY": "VAZIO",
    "UNKNOWN": "DESCONHECIDO",
    "NONE": "NENHUM",
    "HOME": "INICIO",
    "SCREEN": "TELA",
    "GAME": "JOGO",
    "WORLD": "MUNDO",
    "PLAYER": "JOGADOR",
    "DATA": "DADOS",
    "PHONE": "TELEFONE",
    "MOBILE PHONE": "TELEFONE CELULAR",
    "STATS": "STATUS",
    "BATTLE": "BATALHA",
    "BATTLES": "BATALHAS",
    "FIGHT": "LUTAR",
    "RUN": "FUGIR",
    "FORFEIT": "DESISTIR",
    "FORFEITING": "DESISTINDO",
    "FORFEITING LINK BATTLE...": "DESISTINDO DA BATALHA EM LINK...",
    "LEAVING": "SAINDO",
    "LEAVING LINK BATTLE...": "SAINDO DA BATALHA EM LINK...",
    "COULD NOT FORFEIT - TRY AGAIN.":
        "NAO FOI POSSIVEL DESISTIR - TENTE NOVAMENTE.",
    "STRUGGLE COULD NOT BE SENT. TRY AGAIN.":
        "NAO FOI POSSIVEL ENVIAR STRUGGLE. TENTE NOVAMENTE.",
    "TIME UP - MOVE AUTO-SELECTED.":
        "TEMPO ESGOTADO - GOLPE ESCOLHIDO.",
    "TIME UP - POKEMON AUTO-SELECTED.":
        "TEMPO ESGOTADO - POKEMON ESCOLHIDO.",
    "BAG": "MOCHILA",
    "BOX": "CAIXA",
    "PARTY": "EQUIPE",
    "TEAM": "EQUIPE",
    "MOVE": "GOLPE",
    "MOVES": "GOLPES",
    "USED": "USOU",
    "USES": "USA",
    "USE": "USAR",
    "CAUGHT": "CAPTURADO",
    "CATCH": "CAPTURAR",
    "FAINT": "DESMAIAR",
    "FAINTED": "DESMAIOU",
    "SEEN": "VISTO",
    "TRAINER": "TREINADOR",
    "TRAINERS": "TREINADORES",
    "OPPONENT": "OPONENTE",
    "FOE": "OPONENTE",
    "GYM": "GINASIO",
    "LEAGUE": "LIGA",
    "CHAMPION": "CAMPEAO",
    "BADGE": "INSIGNIA",
    "BADGES": "INSIGNIAS",
    "ITEM": "ITEM",
    "ITEMS": "ITENS",
    "HOLDER": "PORTADOR",
    "MONEY": "DINHEIRO",
    "PRICE": "PRECO",
    "BUY": "COMPRAR",
    "SOLD": "VENDIDO",
    "LEFT": "RESTAM",
    "LEVEL": "NIVEL",
    "TYPE": "TIPO",
    "STATUS": "STATUS",
    "ABILITY": "HABILIDADE",
    "ATTACK": "ATAQUE",
    "DEFENSE": "DEFESA",
    "SPEED": "VELOCIDADE",
    "ACCURACY": "PRECISAO",
    "EVASION": "EVASAO",
    "PHYSICAL": "FISICO",
    "SPECIAL": "ESPECIAL",
    "NORMAL": "NORMAL",
    "FIRE": "FOGO",
    "WATER": "AGUA",
    "GRASS": "PLANTA",
    "ELECTRIC": "ELETRICO",
    "ICE": "GELO",
    "FIGHTING": "LUTADOR",
    "POISON": "VENENO",
    "GROUND": "TERRA",
    "FLYING": "VOADOR",
    "PSYCHIC": "PSIQUICO",
    "BUG": "INSETO",
    "ROCK": "PEDRA",
    "GHOST": "FANTASMA",
    "DRAGON": "DRAGAO",
    "DARK": "SOMBRIO",
    "STEEL": "ACO",
    "FAIRY": "FADA",
    "POKEMON": "POKEMON",
    "POKEDEX": "POKEDEX",
    "POKE BALL": "POKE BOLA",
    "GREAT BALL": "GREAT BALL",
    "ULTRA BALL": "ULTRA BALL",
    "MASTER BALL": "MASTER BALL",
    "TACKLE": "INVESTIDA",
    "VINE WHIP": "CHICOTE DE VINHA",
    "SCRATCH": "ARRANHAO",
    "EMBER": "BRASA",
    "WATER GUN": "JATO DAGUA",
    "GUST": "RAJADA",
    "STRING SHOT": "TIRO DE SEDA",
    "POISON STING": "FERROADA VENENOSA",
    "SWORDS DANCE": "DANCA DAS ESPADAS",
    "THUNDERBOLT": "RAIO",
    "SURF": "SURFE",
    "FLY": "VOAR",
    "CUT": "CORTE",
    "STRENGTH": "FORCA",
    "FLASH": "CLARAO",
    "ROCK SMASH": "QUEBRA-ROCHA",
    "SNATCHED": "ROUBADO",
    "TURN": "TURNO",
    "TURNS": "TURNOS",
    "SETTINGS LANGUAGE": "IDIOMA DAS CONFIGURACOES",
    "SAVE PAYLOAD COULD NOT BE LOADED.":
        "NAO FOI POSSIVEL CARREGAR OS DADOS DO JOGO.",
    "Ribombee rolls up pollen into puffs. It makes many different varieties, some used as food and others used in battle. They are sometimes sold as supplements.":
        "RIBOMBEE ENROLA POLEN EM BOLINHAS. PRODUZ MUITAS VARIEDADES, ALGUMAS USADAS COMO ALIMENTO E OUTRAS EM BATALHA. AS VEZES, ELAS SAO VENDIDAS COMO SUPLEMENTOS.",
    "The POKEMON swapped ABILITIES!": "OS POKEMON TROCARAM DE HABILIDADE!",
    "Its scientific name is \"Oddium Wanderus.\" At night, it is said to walk nearly 1,000 feet on its two roots.":
        "SEU NOME CIENTIFICO E \"ODDIUM WANDERUS\". DIZEM QUE, A NOITE, ELE ANDA QUASE 300 METROS SOBRE SUAS DUAS RAIZES.",
    "Single Bloom": "FLOR UNICA",
    "FINAL GAMBIT": "APOSTA FINAL",
    "MILOTIC is breathtakingly beautiful. Those that see it are said to forget their combative spirits.":
        "MILOTIC E DE UMA BELEZA IMPRESSIONANTE. DIZEM QUE QUEM O VE ESQUECE SEU ESPIRITO DE COMBATE.",
    "NIGHT DAZE": "NEVOA NOTURNA",
    "sequence": "SEQUENCIA",
    "< PREV": "< ANTERIOR",
    "BLACKGLASSES": "OCULOS ESCUROS",
    "FLUNG": "ARREMESSADO",
    "The foe's REFLECT raised DEFENSE!": "O REFLETOR DO OPONENTE AUMENTOU A DEFESA!",
    "Further research enhanced its abilities. Sometimes, it may exhibit motions that were not programmed.":
        "OUTRAS PESQUISAS APRIMORARAM SUAS HABILIDADES. AS VEZES, ELE PODE FAZER MOVIMENTOS QUE NAO FORAM PROGRAMADOS.",
    "HUSTLE": "ENTUSIASMO",
    "SCANNING WIFI NETWORKS...": "PROCURANDO REDES WI-FI...",
    "SEARCHING FOR POKEGOCHIS...": "PROCURANDO POKEGOCHIS...",
    "[SDTEST] BEGIN read-only frequency sweep":
        "[SDTEST] INICIAR VARREDURA DE FREQUENCIA SOMENTE LEITURA",
    "SMACK": "BATIDA",
    "WHOLE": "INTEIRO",
    "MAGNET": "IMA",
    "SEQUENCE": "SEQUENCIA",
    "[TEST] HPTEST DONE state-restored=1":
        "[TEST] HPTEST CONCLUIDO ESTADO-RESTAURADO=1",
    "FIGHTINIUM": "FIGHTINIUM",
    "METEOR INDIGO": "METEORO INDIGO",
    "These Pokemon appear at building sites and help out with construction. They always carry squared logs. As they grow, they carry bigger logs.":
        "ESSES POKEMON APARECEM EM OBRAS E AJUDAM NA CONSTRUCAO. ELES SEMPRE CARREGAM VIGAS QUADRADAS. CONFORME CRESCEM, PASSAM A CARREGAR VIGAS MAIORES.",
    "BALANCED - ALL POKEMON Lv70": "BALANCEADO - TODOS POKEMON NV70",
    "AN ODD SHOCK WAVE THAT ONLY DAMAGES SAME-TYPE {PKMN}.":
        "UMA ONDA DE CHOQUE ESTRANHA QUE SO CAUSA DANO A {PKMN} DO MESMO TIPO.",
    "They are intelligent and will cooperate to catch prey. When one Archeops had the prey cornered, another would swoop on it.":
        "ELES SAO INTELIGENTES E COOPERAM PARA CAPTURAR PRESAS. QUANDO UM ARCHEOPS ENCURRALA A PRESA, OUTRO MERGULHA SOBRE ELA.",
    "The small ball is not only filled with nutrients, it is also tasty. STARLY try to peck it off.":
        "A PEQUENA BOLA NAO SO E CHEIA DE NUTRIENTES, COMO TAMBEM E SABOROSA. STARLY TENTAM BICA-LA.",
    "TOTEM GUMSHOOS": "TOTEM GUMSHOOS",
    "When under attack from bird Pokemon, it spews a poisonous black powder that causes paralysis on contact. Scatterbug can live in any region or climate.":
        "QUANDO E ATACADO POR POKEMON PASSARO, ELE SOLTA UM PO PRETO VENENOSO QUE CAUSA PARALISIA AO TOCAR. SCATTERBUG PODE VIVER EM QUALQUER REGIAO OU CLIMA.",
    "Because every PRINPLUP considers itself to be the most important, they can never form a group.":
        "COMO TODO PRINPLUP SE CONSIDERA O MAIS IMPORTANTE, ELES NUNCA CONSEGUEM FORMAR UM GRUPO.",
    "It stuns its prey with its psychic powers and then grinds them to mush with its strong teeth. Even Shellder's shell is no match for it.":
        "ELE ATORDOA A PRESA COM PODERES PSIQUICOS E A TRITURA COM SEUS DENTES FORTES. NEM A CONCHA DE SHELLDER RESISTE.",
    "It conceals itself in the sand and chomps down on the legs of any prey that unwarily walk over it. Its favorite food is Trapinch.":
        "ELE SE ESCONDE NA AREIA E MORDE AS PERNAS DE QUALQUER PRESA DESATENTA QUE PASSE POR CIMA. SUA COMIDA FAVORITA E TRAPINCH.",
    "They use psychic power to control their arms, which are made of a special liquid. When Reuniclus shake hands, a network forms between their brains.":
        "ELES USAM PODER PSIQUICO PARA CONTROLAR OS BRACOS, FEITOS DE UM LIQUIDO ESPECIAL. QUANDO REUNICLUS APERTAM AS MAOS, UMA REDE SE FORMA ENTRE SEUS CEREBROS.",
    "It touches new things with its nose to test for smell and feel. It plays by spinning SPHEAL on its nose.":
        "ELE TOCA COISAS NOVAS COM O NARIZ PARA TESTAR O CHEIRO E A SENSACAO. BRINCA GIRANDO SPHEAL SOBRE O NARIZ.",
    "Their poison is strong enough to eat through the hull of a tanker. Tales are told of ships that wander into seas where Dragalge live, never to return.":
        "SEU VENENO E FORTE O BASTANTE PARA CORROER O CASCO DE UM NAVIO-TANQUE. CONTAM QUE NAVIOS QUE ENTRAM NOS MARES ONDE VIVE DRAGALGE NUNCA RETORNAM.",
    "It can throw bubble-covered pebbles with precise control, hitting empty cans up to a hundred feet away. Frogadier's swiftness is unparalleled.":
        "ELE LANCA PEDRINHAS COBERTAS DE BOLHAS COM PRECISAO E ACERTA LATAS VAZIAS ATE A 30 METROS. A AGILIDADE DE FROGADIER E INCOMPARAVEL.",
    "Strict training and disclipine leads it to master its flowing double-scalchop technique. Scalchop techniques differ from one Dewott to another.":
        "TREINO RIGOROSO E DISCIPLINA O FAZEM DOMINAR SUA TECNICA FLUIDA COM DOIS SCALCHOPS. AS TECNICAS VARIAM DE UM DEWOTT PARA OUTRO.",
    "It draws in air through its tail, transforms it into fire, and uses it like a tongue. They burn through Durant's steel bodies and consume their insides.":
        "ELE PUXA AR PELA CAUDA, TRANSFORMA-O EM FOGO E O USA COMO UMA LINGUA. ELES QUEIMAM O CORPO DE ACO DE DURANT E DEVORAM SEU INTERIOR.",
    "Recognizing their own weakness, they always live in a group. When alone, a STARAVIA cries noisily.":
        "COMO RECONHECEM SUA PROPRIA FRAQUEZA, ELES SEMPRE VIVEM EM GRUPO. QUANDO ESTA SOZINHO, STARAVIA GRITA ALTO.",
    "If it remains still, it becomes impossible to distinguish from real nuts. It delights in surprising foraging PIDGEY.":
        "SE FICAR PARADO, E IMPOSSIVEL DISTINGUI-LO DE NOZES REAIS. ELE ADORA SURPREENDER PIDGEY QUE PROCURAM COMIDA.",
    "This Flabebe rides a red flower. Immediately after birth, this Pokemon begins flying around in search of a flower it likes.":
        "ESTE FLABEBE VIAJA EM UMA FLOR VERMELHA. LOGO APOS NASCER, O POKEMON VOA EM BUSCA DE UMA FLOR DE QUE GOSTE.",
    "Durant dig nests in mountains. Individuals each play different roles in driving Heatmor, their natural predator, away from their colony.":
        "DURANT CAVAM NINHOS NAS MONTANHAS. CADA UM CUMPRE UM PAPEL PARA AFASTAR HEATMOR, SEU PREDADOR NATURAL, DA COLONIA.",
    "A linked cluster formed of several MAGNEMITE. It discharges powerful magnetic waves at high voltage.":
        "UM GRUPO CONECTADO FORMADO POR VARIOS MAGNEMITE. ELE DESCARREGA PODEROSAS ONDAS MAGNETICAS DE ALTA TENSAO.",
    "Myriads of Cutiefly flutter above the heads of people who have auras resembling those of flowers. It can identify which flowers are about to bloom.":
        "MUITOS CUTIEFLY VOAM SOBRE A CABECA DE PESSOAS COM AURAS PARECIDAS COM AS DAS FLORES. ELES IDENTIFICAM QUAIS FLORES ESTAO PRESTES A DESABROCHAR.",
    "This Pokemon appears before people and Pokemon who are having nightmares and eats those dreams. Munna always float in the air.":
        "ESTE POKEMON APARECE DIANTE DE PESSOAS E POKEMON COM PESADELOS E DEVORA ESSES SONHOS. MUNNA SEMPRE FLUTUA NO AR.",
    "Forests where Shiinotic live are treacherous to enter at night. People confused by its strange lights can never find their way home again.":
        "E PERIGOSO ENTRAR A NOITE NAS FLORESTAS ONDE VIVE SHIINOTIC. QUEM SE CONFUNDE COM SUAS LUZES ESTRANHAS NUNCA MAIS ENCONTRA O CAMINHO DE CASA.",
    "Using its dorsal fin as a suction pad, it clings to a MANTINE's underside to scavenge for leftovers.":
        "USANDO A BARBATANA DORSAL COMO VENTOSA, ELE SE PRENDE NA PARTE DE BAIXO DE MANTINE PARA PROCURAR SOBRAS.",
    "When they find a wounded Pokemon, they embrace it and bring it to shore. The special membrane enveloping Alomomola has the ability to heal wounds.":
        "QUANDO ENCONTRAM UM POKEMON FERIDO, ELES O ABRACAM E O LEVAM A PRAIA. A MEMBRANA ESPECIAL QUE ENVOLVE ALOMOMOLA PODE CURAR FERIMENTOS.",
    "The claws on its feet are well developed. It can carry prey such as an EXEGGCUTE to its nest over 60 miles away.":
        "AS GARRAS DE SEUS PES SAO BEM DESENVOLVIDAS. ELE CONSEGUE LEVAR PRESAS COMO EXEGGCUTE A UM NINHO A MAIS DE 95 QUILOMETROS.",
    "Its magnetic nose consistently faces north. Travelers check NOSEPASS to get their bearings.":
        "SEU NARIZ MAGNETICO SEMPRE APONTA PARA O NORTE. VIAJANTES CONSULTAM NOSEPASS PARA SE ORIENTAR.",
}


def strip_compile_only_diagnostics(source: str) -> str:
    """Remove positive animation-diagnostic preprocessor branches.

    Those cable commands never exist in production and are not rendered by
    the game. Treating their serial tokens as player-visible text made the
    dormant localization audit depend on whichever profiler command happened
    to be added most recently.
    """
    output: list[str] = []
    suppressed_depth = 0
    diagnostic_if = re.compile(
        r"^\s*#\s*if\s+defined\(POKEGOCHI_ANIMATION_DIAGNOSTICS\)\s*$"
    )
    nested_if = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef)\b")
    end_if = re.compile(r"^\s*#\s*endif\b")
    for line in source.splitlines(keepends=True):
        if suppressed_depth:
            if nested_if.match(line):
                suppressed_depth += 1
            elif end_if.match(line):
                suppressed_depth -= 1
            continue
        if diagnostic_if.match(line):
            suppressed_depth = 1
            continue
        output.append(line)
    return "".join(output)


def cpp_strings(path: Path) -> list[str]:
    """Extract actual C/C++ string tokens while skipping comments/chars."""
    source = strip_compile_only_diagnostics(
        path.read_text(encoding="utf-8", errors="replace")
    )
    values: list[str] = []
    index = 0
    size = len(source)
    while index < size:
        if source.startswith("//", index):
            end = source.find("\n", index + 2)
            index = size if end < 0 else end + 1
            continue
        if source.startswith("/*", index):
            end = source.find("*/", index + 2)
            index = size if end < 0 else end + 2
            continue
        if source[index] == "'":
            index += 1
            while index < size:
                if source[index] == "\\":
                    index += 2
                elif source[index] == "'":
                    index += 1
                    break
                else:
                    index += 1
            continue
        if source[index] != '"':
            index += 1
            continue
        index += 1
        raw: list[str] = []
        while index < size:
            if source[index] == "\\" and index + 1 < size:
                raw.extend((source[index], source[index + 1]))
                index += 2
            elif source[index] == '"':
                index += 1
                break
            else:
                raw.append(source[index])
                index += 1
        try:
            values.append(ast.literal_eval('"' + "".join(raw) + '"'))
        except (SyntaxError, ValueError):
            continue
    return values


def ascii_portuguese(value: str) -> str:
    value = value.replace("POKÉMON", "POKEMON").replace("Pokémon", "POKEMON")
    value = value.replace("’", "'").replace("–", "-").replace("—", "-")
    value = unicodedata.normalize("NFKD", value)
    value = "".join(character for character in value
                    if not unicodedata.combining(character))
    value = value.encode("ascii", "ignore").decode("ascii")
    value = re.sub(r"[ \t]+", " ", value)
    return value.upper().strip()


def prepare_ptbr_source(value: str) -> str:
    """Disambiguate recurring game terms before generic EN -> PT-BR MT."""
    prepared = value
    # In Pokemon UI, PARTY always means the battle roster, never a celebration
    # or political party. TEAM consistently produces Brazilian `equipe`.
    prepared = re.sub(r"\bparty\b", "team", prepared,
                      flags=re.IGNORECASE)
    # Tackle in move prose is a charge, not equipment or a fishing tackle.
    prepared = re.sub(r"\btackle\b", "charge attack", prepared,
                      flags=re.IGNORECASE)
    # Explicit battle context avoids translating a turn as a lap/return.
    prepared = re.sub(r"\bturns\b", "battle turns", prepared,
                      flags=re.IGNORECASE)

    # SAVE is a noun in these protocol/UI phrases. Generic translation models
    # otherwise read it as the imperative verb `salvar`.
    noun_phrases = (
        "save file", "save export", "save import", "save backup",
        "save payload", "save reset", "save recovery", "save management",
        "save storage", "save layout", "save reservation",
    )
    for phrase in noun_phrases:
        replacement = phrase.replace("save", "game-data", 1)
        prepared = re.sub(rf"\b{re.escape(phrase)}\b", replacement,
                          prepared, flags=re.IGNORECASE)
    prepared = re.sub(r"\binvalid save\b", "invalid game data", prepared,
                      flags=re.IGNORECASE)
    prepared = re.sub(r"\bsave is busy\b", "game data is busy", prepared,
                      flags=re.IGNORECASE)
    return prepared


def finalize_ptbr(english: str, portuguese: str) -> str:
    result = ascii_portuguese(portuguese)
    substitutions = {
        "EQUIPA": "EQUIPE",
        "EQUIPAS": "EQUIPES",
        "FICHEIRO": "ARQUIVO",
        "FICHEIROS": "ARQUIVOS",
        "TELEMOVEL": "CELULAR",
        "TELEMOVEIS": "CELULARES",
        "ECRA": "TELA",
        "ECRAS": "TELAS",
        "RAPARIGA": "GAROTA",
        "RAPARIGAS": "GAROTAS",
        "AUTOCARRO": "ONIBUS",
        "AUTOCARROS": "ONIBUS",
        "DESPORTO": "ESPORTE",
        "DESPORTOS": "ESPORTES",
    }
    for european, brazilian in substitutions.items():
        result = re.sub(rf"\b{european}\b", brazilian, result)
    result = result.replace("PEQUENO-ALMOCO", "CAFE DA MANHA")
    result = result.replace("CASA DE BANHO", "BANHEIRO")
    result = re.sub(r"\bTURNOS? DE BATALHA\b",
                    lambda match: "TURNOS" if match.group(0).startswith("TURNOS")
                    else "TURNO", result)
    result = re.sub(r"\bPOKEMONS\b", "POKEMON", result)

    if re.search(r"\bPARTY\b", english, flags=re.IGNORECASE):
        result = re.sub(r"\b(?:FESTA|PARTIDO|GRUPO|TIME)\b", "EQUIPE", result)
    return result


def visible_candidate(value: str) -> bool:
    if not (2 <= len(value) <= 700) or not re.search(r"[A-Za-z]", value):
        return False
    lowered = value.lower()
    blocked = (
        "/pokegochi/", ".pkg", ".bin", ".json", ".pgota", ".local",
        "http://", "https://", "-----begin", "_front_pic", "assets/",
        "reason=", "heap=", "frame=", "checksum=", "uid=", "screen=",
        "phase=", "event=", "source=", "selected=", "failed:", "[boot]",
        "[gfx]", "[ble]", "[wifi]", "[link]", "[android]", "[battle]",
        "[save]", "[conf", "[hp_", "[ui]", "[sd]", "[touch]",
    )
    if any(fragment in lowered for fragment in blocked):
        return False
    if "%" in value or "\\x" in value:
        return False
    if re.fullmatch(r"[A-Z0-9]+(?:_[A-Z0-9]+)+", value.strip()):
        return False
    if re.fullmatch(r"[a-z0-9_.-]+", value) and ("_" in value or "." in value):
        return False
    return True


def collect_catalogue() -> tuple[set[str], Counter[str]]:
    exact: set[str] = set(MANUAL)
    dynamic_words: Counter[str] = Counter()

    source_files = [ROOT / "src/main.cpp"]
    source_files += sorted((ROOT / "src/game").glob("*.cpp"))
    source_files += sorted((ROOT / "src/services").glob("*.cpp"))
    source_files += sorted((ROOT / "include/game").glob("*.h"))
    for path in source_files:
        for value in cpp_strings(path):
            internal_identifier = re.fullmatch(
                r"[A-Z0-9]+(?:_[A-Z0-9]+)+", value.strip()
            )
            if (re.search(r"[A-Za-z]", value) and not internal_identifier and
                    not value.lstrip().startswith("[")):
                dynamic_words.update(word.upper() for word in
                                     re.findall(r"[A-Za-z]+", value))
            if visible_candidate(value):
                exact.add(value)

    # The Pokedex is wholly player-visible: category plus all 807 entries.
    for value in cpp_strings(ROOT / "src/game/PokedexDataGenerated.inc"):
        if visible_candidate(value):
            exact.add(value)

    # Move names are rendered in Battle, Summary, learnsets, Bag and Mart.
    move_source = (ROOT / "src/game/MoveDataGenerated.inc").read_text(
        encoding="utf-8")
    exact.update(re.findall(
        r"\{\s*\d+\s*,\s*\"([^\"]+)\"\s*,\s*PokemonType::", move_source))
    post_move_source = (ROOT / "src/game/PokemonData.cpp").read_text(
        encoding="utf-8")
    exact.update(re.findall(
        r"\{\s*\d+\s*,\s*\"([^\"]+)\"\s*,\s*PokemonType::", post_move_source))

    # Importing the existing generator gives the exact decompressed strings
    # seen by moveDescription(), while leaving normal builds network-free.
    sys.path.insert(0, str(ROOT / "scripts"))
    generated = runpy.run_path(str(ROOT / "scripts/generate_move_data.py"))
    exact.update(generated["descriptions"].values())

    # Abilities are the only visible strings in PokemonDataGenerated; species
    # names deliberately remain canonical in every language.
    pokemon_data = (ROOT / "src/game/PokemonDataGenerated.inc").read_text(
        encoding="utf-8")
    ability_start = pokemon_data.index("constexpr AbilityData kAbilities[]")
    ability_source = pokemon_data[ability_start:]
    for _number, name, description in re.findall(
            r"\{(\d+),\s*\"((?:\\.|[^\"\\])*)\",\s*"
            r"\"((?:\\.|[^\"\\])*)\"\}", ability_source):
        exact.add(ast.literal_eval('"' + name + '"'))
        exact.add(ast.literal_eval('"' + description + '"'))

    # Dynamic battle messages contain species names as ordinary words. Never
    # send those tokens through a translation model (for example, PIKACHU
    # must not become PICACHU).
    species_section = pokemon_data[:ability_start]
    species_names = set(re.findall(
        r'^\s*\{"([A-Z0-9 .\-]+)",\s*\d+,', species_section,
        flags=re.MULTILINE))
    species_words = {
        word for name in species_names for word in re.findall(r"[A-Z]+", name)
    }

    # Trainer names remain names; class and the combined two-line dialogue
    # are localized exactly as drawTrainerIntro() presents them.
    trainer_source = (ROOT / "src/game/TrainerDataGenerated.inc").read_text(
        encoding="utf-8")
    trainer_pattern = re.compile(
        r"\{\d+,\s*\"([^\"]*)\",\s*\"[^\"]*\",\s*\"[^\"]*\",\s*"
        r"\"((?:\\.|[^\"\\])*)\",\s*\"((?:\\.|[^\"\\])*)\"")
    for trainer_class, first, second in trainer_pattern.findall(trainer_source):
        line1 = ast.literal_eval('"' + first + '"')
        line2 = ast.literal_eval('"' + second + '"')
        exact.add(trainer_class)
        exact.add(line1 + ("\n" + line2 if line2 else ""))

    # Translate words used in formatted runtime strings. Exact prose above
    # does not need a word-by-word duplicate, which keeps flash use bounded.
    for word in dynamic_words:
        if (len(word) >= 2 and
                word not in {"HTTP", "HTTPS", "PKG", "JSON"} and
                (word not in species_words or word in MANUAL)):
            exact.add(word)
    return exact, dynamic_words


def existing_catalogue() -> dict[str, str]:
    cached: dict[str, str] = {}
    if (OUT.exists() and
            CATALOG_ENGINE_VERSION in OUT.read_text(encoding="utf-8")[:300]):
        pattern = re.compile(
            r"\{0x[0-9A-F]{8}U,\s*(\"(?:\\.|[^\"\\])*\"),\s*"
            r"(\"(?:\\.|[^\"\\])*\")\},")
        for english, portuguese in pattern.findall(OUT.read_text(encoding="utf-8")):
            cached[ast.literal_eval(english)] = ast.literal_eval(portuguese)
    if CACHE.exists():
        cached.update(json.loads(CACHE.read_text(encoding="utf-8")))
    return cached


def ensure_local_translation_model() -> None:
    LOCAL_MODEL.mkdir(parents=True, exist_ok=True)
    for name in ("config.json", "model.bin", "source.spm", "target.spm",
                 "shared_vocabulary.json"):
        destination = LOCAL_MODEL / name
        if destination.exists() and destination.stat().st_size:
            continue
        print(f"Downloading local EN-PT model component {name}...")
        urllib.request.urlretrieve(LOCAL_MODEL_BASE + name, destination)


def translate_locally(values: list[str], persistent: dict[str, str]) -> dict[str, str]:
    if not LOCAL_RUNTIME.exists():
        raise RuntimeError(
            "Local CTranslate2 runtime is missing. Install ctranslate2 and "
            "sentencepiece into .artifacts/ct2-runtime.")
    sys.path.insert(0, str(LOCAL_RUNTIME))
    ensure_local_translation_model()
    import ctranslate2  # type: ignore
    import sentencepiece as sentencepiece_module  # type: ignore

    source_processor = sentencepiece_module.SentencePieceProcessor(
        model_file=str(LOCAL_MODEL / "source.spm"))
    target_processor = sentencepiece_module.SentencePieceProcessor(
        model_file=str(LOCAL_MODEL / "target.spm"))
    translator = ctranslate2.Translator(
        str(LOCAL_MODEL), device="cpu", compute_type="int8",
        inter_threads=2, intra_threads=4)
    result: dict[str, str] = {}
    # Similar lengths belong together. CTranslate2 pads a batch to its longest
    # sequence; alphabetic ordering previously made a 700-character Pokedex
    # paragraph stall dozens of one-word UI labels in the same batch.
    values = sorted(values, key=lambda value: (len(value), value))
    batch_size = 64
    for start in range(0, len(values), batch_size):
        batch = values[start:start + batch_size]
        # The multilingual OPUS model distinguishes regional variants at the
        # source. `pob` is Brazilian Portuguese; `por` would select European
        # Portuguese and must never be used for this game.
        tokenized = []
        for value in batch:
            # The model was trained on normal sentence casing. Feeding UI
            # labels in ALL CAPS can turn ordinary words such as SCREEN into
            # unrelated verbs; output is uppercased later for the game font.
            prepared = prepare_ptbr_source(value)
            model_value = prepared.lower() if value.upper() == value else prepared
            tokenized.append(
                [">>pob<<"] +
                source_processor.encode(model_value, out_type=str) +
                ["</s>"]
            )
        translated_batch = translator.translate_batch(
            tokenized, beam_size=3, max_decoding_length=512)
        for english, translated_result in zip(batch, translated_batch):
            decoded = target_processor.decode(
                translated_result.hypotheses[0]
            ).strip()
            result[english] = finalize_ptbr(english, decoded)
        persistent.update(result)
        CACHE.parent.mkdir(parents=True, exist_ok=True)
        CACHE.write_text(json.dumps(persistent, ensure_ascii=False,
                                    sort_keys=True), encoding="utf-8")
        print(f"Local translation {min(start + batch_size, len(values))}/"
              f"{len(values)}")
    return result


def fnv1a(value: str) -> int:
    result = 2166136261
    for byte in value.encode("utf-8"):
        result ^= byte
        result = (result * 16777619) & 0xFFFFFFFF
    return result


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def main() -> None:
    exact, words = collect_catalogue()
    cached = existing_catalogue()
    translated = {key: value for key, value in cached.items() if key in exact}
    translated.update(MANUAL)
    missing = sorted(value for value in exact if value not in translated)
    print(f"Catalogue: {len(exact)} strings, {len(words)} formatted words, "
          f"{len(missing)} new translations")
    if missing:
        translated.update(translate_locally(missing, translated))

    # Empty responses are never useful and would make the English UI vanish.
    for english in exact:
        if not translated.get(english):
            translated[english] = ascii_portuguese(english)
    rows = sorted(((fnv1a(english), english, translated[english])
                   for english in exact), key=lambda row: (row[0], row[1]))
    lines = [
        "// Generated by scripts/generate_portuguese_localization.py.",
        f"// Translation engine: {CATALOG_ENGINE_VERSION}.",
        "// Sorted FNV-1a catalogue for allocation-free binary lookup.",
        "constexpr TranslationEntry kPortugueseTranslations[] = {",
    ]
    lines.extend(
        f"  {{0x{hash_value:08X}U, {cpp_string(english)}, "
        f"{cpp_string(portuguese)}}},"
        for hash_value, english, portuguese in rows)
    lines.append("};")
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    if CACHE.exists():
        CACHE.unlink()
    print(f"Generated {OUT.relative_to(ROOT)} with {len(rows)} translations")


if __name__ == "__main__":
    main()
