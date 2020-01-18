# ISA - Síťové aplikace a správa sítí - Jednoduchý LDAP klient

### Zadání:

Vytvořte komunikující aplikaci podle konkrétní vybrané specifikace pomocí síťové knihovny BSD sockets. Projekt bude vypracován v jazyce C/C++, přeložitelný a spustitelný v prostředí FreeBSD (např. server eva.fit.vutbr.cz). 

Vašim úkolem je nastudovat si protokol LDAP, který zasílá data ve standardu ASN.1 kódovaná pomocí BER a následně vytvořit jednoduchého LDAP klienta, který bude zasílat LDAP žádosti na vyhledání osob v adresáři FIT VUT a bude zpracovávat odpovědi serveru.
 
Vaše aplikace by měla minimálně implementovat navázání spojení se serverem (bindRequest), zaslání dotazu (searchRequest) a korektně se od serveru odhlásit (unbindRequest). Veškerá podstatná nastavení aplikace (host, port, filtr, hloubka, atributy výsledků vyhledání) budou uložené v externím plaintextovém souboru s předem danou stukturou. Aplikace nemusí řešit autentizaci.
 
Vaše aplikace by měla být POUŽITELNÁ a SMYSLUPLNÁ využívající BSD sokety a umožňující během jednoho svého běhu položit i více dotazů na LDAP server, jejichž výsledky pak INTUITIVNĚ interpretuje uživateli.
 
**Závazná sruktura souboru s nastavením:**

    [host:port]
    <adresa LDAP serveru>:<port se službou>
    [base]
    <startovací uzel pro vyhledávání>
    [depth]
    <hloubka>
    [filter]
    <filtr omezení vyhledání>
    [result]
    <seznam výsledků vyhledání>
 
Parametr [base] slouží pro searchRequest jako hodnota pole baseObject.\
Parametr hloubky [depth] nabývá jedné z následujících hodnot [base|one|sub].\
Parametr [filter] pracuje s jednoduchou nezanořenou relační logikou se symboly:

- & jakožto AND
- | jakožto OR
- ! jakožto NOT
- = jakožto rovnost
- \* jakožto libovolný řetězec
- %s jakožto proměnnou, kterou bude vaše aplikace substituovat za uživatelem vyhledávaný řetězec

Nelekejte se, když si prostudujete LDAP filtrování v rámci zprávy searchRequest, bude vám vše jasnější. Příklady složeného filtru v notaci konfiguračního souboru:
 
- uid=\*%s\*&cn=\*Jan\*
- cn=\*%s\*&ou=!UIFS

Parametr [result] pak bude obsahovat mezerami oddělené LDAP atributy, které chcete v daném pořadí zobrazovat jako výsledek vyhledání.
 
**Povinný vstup:**

    $./myldapsearch <soubor s nastavením> 
 
**Možný příklad běhu:**

    $./cat config.txt
    [host:port]
    ldap.fit.vutbr.cz:389
    [base]
    dc=fit,dc=vutbr,dc=cz
    [depth]
    one
    [filter]
    uid=*%s*
    [result]
    uid cn
    $./myldapsearch config.txt
    
    Vložte řetězec: xbera
    Výsledek:
    xberan18 | Beran Petr
    xberan24 | Beránek Jan
    xberan25 | Beran Milan
    xberan28 | Beran Martin
    xberan29 | Beran Lukáš
    
    Vložte řetězec: ...
 
**Dodatky/doporučení:**

- nesmí se použít knihovna OpenLDAP, tedy ldap.h a lber.h, bez toho by to nebyla žádná výzva
- hodnotu pole derefAliases nastavte na neverDerefAliases
- ukončení činnosti vaší aplikace (a korektní odeslání unbindRequest) můžete navázat třeba vymaskováním Ctrl+C interruptu či jinou vhodnou metodou
- nainstalujte si WireShark a nakonfigurujte si svůj poštovní klient (Thunderbird, Outlook) na použití adresářové služby; získáte tak snadnou referenční aplikaci, na které můžete odchytáváním paketů zkoumat protokol LDAP

**Literatura:**

- problematika filtrů v RFC4515 - LDAP String Representation of Search Filters (http://tools.ietf.org/html/rfc4515) 
- RFC1777 - Lightweight Directory Access Protocol (http://tools.ietf.org/html/rfc1777) a související RFC dokumenty
- ASN.1 Encoding Rules (http://www.oss.com/asn1/organizations.html)
- Basic Encoding Rules (http://www.itu.int/ITU-T/studygroups/com17/languages/X.690-0207.pdf)

### Dokumentace: 

[manual.pdf](manual.pdf)
    
### Hodnocení: 

**20/22**