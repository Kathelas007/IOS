# EN
## Description
Script for server web recors analysis. Script filters records and creates statistics. 
# CZ
## Popis úlohy
Cílem úlohy je vytvořit skript pro analýzu záznamů webového serveru. Skript
bude filtrovat záznamy a poskytovat statistiky podle zadání úživatele.
Specifikace chování skriptu
### JMÉNO
• wana - analyzátor webových logů
### POUŽITÍ
• wana [FILTR] [PŘÍKAZ] [LOG [LOG2 [...]]
### VOLBY
* PŘÍKAZ může být jeden z: 
– list-ip – výpis seznamu zdrojových IP adres.
– list-hosts – výpis seznamu zdrojových doménových jmen.
– list-uri – výpis seznamu cílových zdrojů (URI).
– hist-ip – výpis histogramu četností dotazů podle zdrojových IP
adres.
– hist-load – výpis histogramu zátěže (tj. počtu dotazů ve jed-
notlivých časových intervalech).

* FILTR může být kombinace následujících:
– -a DATETIME – after = jsou uvažovány pouze záznamy PO tomto
datu (bez tohoto data). DATETIME je formátu YYYY-MM-DD HH:MM:SS.
– -b DATETIME – before, jsou uvažovány pouze záznamy PŘED tímto
datem (bez tohodo data).
– -ip IPADDR – jsou uvažovány pouze záznamy odpovídající poža-
davkům ze zdrojové adresy IPADDR. Formát IPADDR odpovídá IPv4
nebo IPv6.
– -uri URI – jsou uvažovány pouze záznamy týkající se dotazů na
konkrétní webovou stránku. URI je základní regulární výraz.
