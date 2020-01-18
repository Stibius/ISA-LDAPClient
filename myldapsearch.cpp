#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <iostream>
#include <fstream>
#include <iterator>
#include <signal.h>
#include <climits>
#include <cstring>

using namespace std;

//globalni promenne
int id;  //id LDAP zpravy 

//LDAP konstanty
constexpr char INTEGER = 0x02;
constexpr char STRING = 0x04;
constexpr char ENUMERATED = 0x0A;
constexpr char SEQUENCE = 0x30;
constexpr char INITIAL = 0x80;
constexpr char ANY = 0x81;
constexpr char FINAL = 0x82;
constexpr char AND = 0xA0;
constexpr char OR = 0xA1;
constexpr char NOT = 0xA2;
constexpr char EQMATCH = 0xA3;
constexpr char SUBSTRINGS = 0xA4;
constexpr char SET = 0x31;
constexpr char AP1 = 0x61;
constexpr char AP3 = 0x63;
constexpr char AP4 = 0x64;
constexpr char AP5 = 0x65;

//bindrequest
constexpr unsigned char BINDREQUEST[15] = {0x30,0x81,0x0C,0x02,0x01,0x00,0x60,0x07,0x02,0x01,0x02,0x04,0x00,0x80,0x00};

//precte konfiguracni soubor a vytahne z nej potrebne informace, pri neuspechu vraci -1
//char filename[] - nazev souboru
int config(char filename[], string& host, int& port, string& base, int& depth, string& filter, string& result) 
{
    char c;
    string depths; //retezcova reprezentace parametru depth
    ifstream file;

    host = "";
    port = 0;
    base = "";
    filter = "";
    result = "";

    file.open(filename);
    if (file.fail()) return -1; //soubor se nepodarilo otevrit

    //konfiguracni soubor ma 9 radku
    for (int i = 0; i <= 9; ++i) 
    {
        if (file.eof()) return -1; //predcasny konec souboru
        switch(i) 
        {
            case 0:
            case 2:
            case 4:
            case 6: 
            case 8:
                //sude radky preskakujeme
                while (file.get() != '\n' && !file.eof());
                break;
            case 1:
                //nacitame jmeno hosta pred dvojteckou
                while ((c = file.get()) != ':' && !file.eof()) 
                {
                    host += c;    
                }
                //nacitame a pocitame cislo portu za dvojteckou
                while ((c = file.get()) != '\n' && !file.eof()) 
                {
                    port *= 10;
                    port += c - '0';    
                }  
                break;
            case 3:
                //nacitame base
                while ((c = file.get()) != '\n' && !file.eof()) 
                {
                    base += c;
                }
                break; 
            case 5:
                //nacitame depth do retezce
                while ((c = file.get()) != '\n' && !file.eof()) 
                {
                    depths += c;
                }
                //nastavime integer promennou podle nacteneho retezce
                if (depths.compare("base") == 0) 
                {
                    depth = 0;
                }
                else if (depths.compare("one") == 0) 
                {
                    depth = 1;
                }
                else if (depths.compare("sub") == 0) 
                {
                    depth = 2;
                }
                else return -1; //neni tam jedna ze tri validnich hodnot
                break;   
            case 7:
                //nacitame filter
                while ((c = file.get()) != '\n' && !file.eof()) 
                {
                    filter += c;
                }
                break; 
            case 9:
                //nacitame result
                while ((c = file.get()) != '\n' && !file.eof()) 
                {
                    result += c;
                }
                break; 
        }
    }
   
    file.close();    
    return 0;
}

//prevede filter z infixove do prefixove podoby a doplni zavorky
//vraci novy filter
//vystup je filter tvaru napr.: (&(!(cn=*Jan*))(&(uid=*xvybir*)(!(cn=*Radim*)))) 
string toprefix(string filter) 
{
     int i = 0; //aktualni pozice ve filteru
     int m = 0; //pozice, kam se premisti operator &, | nebo !, pokud na nej narazime
     bool n = false; //indikator, zda aktualni vyraz je negace

     //prochazeni filteru
     while (1) 
     {
         if (i == 0) filter.insert(filter.begin(), '('); //pocatecni zavorka vyrazu
         if (filter[i] == '&' || filter[i] == '|') 
         {
             filter.insert(filter.begin()+m, filter[i]); //pred vyraz
             filter.insert(filter.begin()+m, '(');       //pred vyraz
             i += 2; 
             filter.insert(filter.end(), ')'); //koncova zavorka od (&
             filter[i] = ')'; //koncova zavorka vyrazu
             i++;
             if (n == true) 
             {
                 //byla to negace, je treba jeste jednu zavorku
                 filter.insert(filter.begin()+i, ')');
                 i++;
                 n = false;
             }
             m = i; //nastaveni noveho m
             filter.insert(filter.begin()+i, '('); //pocatecni zavorka dalsiho vyrazu
         }
         else if (filter[i] == '!') 
         {
             filter.insert(m, "(!"); //pred vyraz  
             i += 2; 
             filter.erase(filter.begin()+i); //odstrani ! z puvodniho mista
             i--;
             n = true; //indikuje, ze aktualni vyraz je negace
         }
         else if (filter[i] == ')' && n == true) //konci vyraz
         {
             //byla to negace, je treba jeste jednu zavorku
             filter.insert(filter.begin()+i, ')');
             n = false;
         }
         else if (filter[i] == '\0') //konec
         {
             if (n == true) 
             {
                 //byla to negace, je treba jeste jednu zavorku
                 filter.insert(filter.begin()+i, ')'); 
             }
             filter.insert(filter.end(), ')'); 
             break;
         }
         i++;
     }
  
     return filter;         
}

//zpracuje cislo do formatu, ve kterem se bude vyskytovat v LDAP zprave, a vrati vysledek
string divide_number(int number) 
{
    string divided_number = "";
    int remainder;
    do 
    {
        remainder = number % static_cast<int>(UCHAR_MAX+1);
        divided_number.insert(divided_number.begin(), static_cast<char>(remainder));
        number = number / static_cast<int>(UCHAR_MAX+1);  
    } while (number != 0);
    
    return divided_number;   
}

//vlozi na zacatek generovane LDAP zpravy informaci o delce z integer promenne
void divide_length(string& message, int value) 
{
    string length = divide_number(value); //zpracuje cislo do pozadovaneho formatu
    //pokud informace o delce zabira vic bytu, vlozi se informace o jejich poctu
    if (length.size() > 1) length.insert(length.begin(), 0x80 + length.size());
    //vlozi se samotna informace o delce
    message.insert(0, length);      
}

//zakoduje filter podle BER kodovani a vrati vysledek
string encode(string filter) 
{
    int j; //pomocna promenna
    int state = 0;
    string result = ""; //vysledny zakodovany filter

    //prevede filter z infixove do prefixove formy a prida zavorky
    filter = toprefix(filter);

    int i = filter.size() - 1; //index se nastavi na konec filteru
    //postupujeme od konce k zacatku
    while (i >= 0) 
    {
        j = 0;
        switch (state) 
        {
            case 0: //za vyrazem neco=neco
                if (filter[i] == ')') 
                {
                    state = 0;
                }
                else if (filter[i] == '*') 
                {
                    state = 3; //substrings
                }
                else 
                {
                    result = filter[i] + result; //vlozeni znaku z filteru
                    state = 2; //zatim jsme nenarazili na *, takze to nemusi byt substrings
                }
                break;
            case 1: //jsme pred (neco=neco), budou tam operatory
                if (filter[i] == '(') 
                {
                    state = 1; //hledame dalsi operatory 
                }
                else if (filter[i] == '&') 
                {
                    divide_length(result, result.size()); //vlozi informaci o delce
                    result.insert(result.begin(), AND);   //vlozi typ
                    state = 1; //hledame dalsi operatory
                }
                else if (filter[i] == '|') 
                {
                    divide_length(result, result.size()); //vlozi informaci o delce
                    result.insert(result.begin(), OR);    //vlozi typ
                    state = 1; //hledame dalsi operatory
                }
                else if (filter[i] == '!') 
                {
                    //pocitani velikosti negace, ktera muze koncit jednim z:
                    //konec, and, or, eqmatch, substrings, not
                    while (j <= result.size()-1 && result[j] != AND && result[j] != OR && result[j] != EQMATCH && result[j] != SUBSTRINGS && result[j] != NOT || j == 0) 
                    { 
                        j++;  
                    }
                    divide_length(result, j);           //vlozi informaci o delce
                    result.insert(result.begin(), NOT); //vlozi typ
                    state = 1; //hledame dalsi operatory
                }
                else if (filter[i] == ')') //je tam dalsi vyraz neco=neco
                {
                    state = 0; //za vyrazem neco=neco
                }
                break;
            case 2: //ve vyrazu neco=neco, zatim jsme nenarazili na *, takze to nemusi byt substrings
                if (filter[i] == '*') 
                {
                    //pocitani velikosti prave nactene casti, ktera muze koncit jednim z:
                    //konec, and, or, eqmatch, substrings, not
                    while (j <= result.size()-1 && result[j] != AND && result[j+1] != OR && result[j] != EQMATCH && result[j] != SUBSTRINGS && result[j] != NOT) 
                    { 
                        j++;  
                    } 
                    divide_length(result, j);             //vlozi informaci o delce
                    result.insert(result.begin(), FINAL); //vlozi typ 
                    state = 3; //substrings
                }
                else if (filter[i] == '=') 
                {
                    //pocitani velikosti prave nactene casti, ktera muze koncit jednim z:
                    //konec, and, or, eqmatch, substrings, not
                    while (j <= result.size()-1 && result[j] != AND && result[j] != OR && result[j] != EQMATCH && result[j] != SUBSTRINGS && result[j] != NOT) 
                    { 
                        j++;  
                    } 
                    divide_length(result, j);              //vlozi informaci o delce
                    result.insert(result.begin(), STRING); //vlozi typ
                    state = 2; //ve vyrazu neco=neco, zatim jsme nenarazili na *, takze to nemusi byt substrings
                }
                else if (filter[i] == '(') //konci vyraz neco=neco
                {
                    //pocitani velikosti prave nactene casti, ktera muze koncit jednim z:
                    //konec, string
                    while (j <= result.size()-1 && result[j] != STRING) 
                    { 
                        j++;  
                    } 
                    divide_length(result, j);              //vlozi informaci o delce
                    result.insert(result.begin(), STRING); //vlozi typ
                    j = 0;
                    //pocitani velikosti prave nactene casti, ktera muze koncit jednim z:
                    //konec, and, or, eqmatch, substrings, not
                    while (j <= result.size()-1 && result[j] != AND && result[j] != OR && result[j] != EQMATCH && result[j] != SUBSTRINGS && result[j] != NOT) 
                    { 
                        j++;  
                    }
                    divide_length(result, j);               //vlozi informaci o delce
                    result.insert(result.begin(), EQMATCH); //vlozi typ
                    state = 1; //jsme pred (neco=neco), budou tam operatory                
                }
                else 
                {
                    //jde o pismeno, ktere se normalne vlozi
                    result = filter[i] + result;   
                }
                break;
            case 3: //substrings
                if (filter[i] == '*') 
                {
                    //pocitani velikosti prave nactene casti, ktera muze koncit jednim z:
                    //konec, and, or, eqmatch, substrings, any, final, not
                    while (j <= result.size()-1 && result[j] != AND && result[j] != OR && result[j] != EQMATCH && result[j] != SUBSTRINGS && result[j] != ANY && result[j] != FINAL && result[j] != NOT) 
                    { 
                        j++;  
                    } 
                    divide_length(result, j);           //vlozi informaci o delce
                    result.insert(result.begin(), ANY); //vlozi typ
                    state = 3; //substrings
                }
                else if (filter[i] == '=') 
                {
                    if (filter[i+1] != '*') //pokud druhy znak je *, tato cast jiz byla vlozena
                    {
                        //pocitani velikosti prave nactene casti, ktera muze koncit jednim z:
                        //any, final, and, or, not, substrings, eqmatch
                        while (j <= result.size()-1 && result[j] != ANY && result[j] != FINAL && result[j] != AND && result[j] != OR && result[j] != EQMATCH && result[j] != SUBSTRINGS && result[j] != NOT) 
                        { 
                            j++;  
                        } 
                        divide_length(result, j);               //vlozi informaci o delce
                        result.insert(result.begin(), INITIAL); //vlozi typ
                    }
                    j = 0;
                    //pocitani velikosti prave nactene casti, ktera muze koncit jednim z:
                    //konec, and, or, eqmatch, substrings, not
                    while (j <= result.size()-1 && result[j] != AND && result[j] != OR && result[j] != EQMATCH && result[j] != SUBSTRINGS && result[j] != NOT) 
                    { 
                        j++;  
                    } 
                    divide_length(result, j);                //vlozi informaci o delce
                    result.insert(result.begin(), SEQUENCE); //vlozi typ
                    state = 3; //substrings
                }
                else if (filter[i] == '(') //konci vyraz neco=neco
                {
                    //pocitani velikosti prave nactene casti, ktera muze koncit jednim z:
                    //konec, sequence
                    while (j <= result.size()-1 && result[j] != SEQUENCE) 
                    { 
                        j++;  
                    } 
                    divide_length(result, j);              //vlozi informaci o delce
                    result.insert(result.begin(), STRING); //vlozi typ
                    j = 0;
                    //pocitani velikosti prave nactene casti, ktera muze koncit jednim z:
                    //konec, and, or, eqmatch, substrings, not
                    while (j <= result.size()-1 && result[j] != AND && result[j] != OR && result[j] != EQMATCH && result[j] != SUBSTRINGS && result[j] != NOT) 
                    {
                        j++;  
                    }
                    divide_length(result, j);                  //vlozi informaci o delce
                    result.insert(result.begin(), SUBSTRINGS); //vlozi typ                
                    state = 1; //beginning
                }
                else 
                {
                    //jde o pismeno, ktere se normalne vlozi
                    result = filter[i] + result;   
                }
                break;    
        }
        i--;
    }

    return result;
}

//ziska a vrati informaci o delce, ktera muze byt rozdelena na vice bytu v LDAP zprave, jako integer
//divided_length - cast LDAP zpravy obsahujici udaj o velikosti
int getlength(string divided_length) 
{
    int length = 0;
    int power = UCHAR_MAX+1;
    for (int i = divided_length.size()-1; i != 0; --i) 
    {
        length += divided_length[i];
        if (i != divided_length.size()-1) 
        {
            length += power;
            power *= UCHAR_MAX+1;
        }     
    }

    return length;
}

//preskoci v LDAP zprave informaci o delce
//pocatecni pozice v i: type
//length: ulozi se tam samotna delka
//vrati prvni pozici za informaci o delce
int skip_length(string message, int i, int& length) 
{
    length = 0;
    i++;
    if (message[i] >= static_cast<char>(0x81) && message[i] <= static_cast<char>(0x89)) 
    {
        //hodnota delky je rozdelena na vic bytu
        int length_of_length = static_cast<unsigned char>(message[i]) - 0x80;
        string divided_length = ""; //ziskame rozdelenou hodnotu
        for (int p = 0; p < length_of_length; ++p) 
        {
            i++;
            divided_length += message[i]; 
        }
        length = getlength(divided_length); //prevedeme na integer
    }
    else 
    {
        length = message[i]; //hodnota delky je jen jeden byte
    }

    return ++i;
}

//preskoci v LDAP zprave celou jednu polozku (type, length i value)
//pocatecni pozice v i: type
//vrati prvni pozici za preskocenou polozkou
int skip_tlv(string message, int i) 
{
    int length;
    i++;
    if (message[i] >= static_cast<char>(0x81) && message[i] <= static_cast<char>(0x89)) 
    {
        //hodnota delky je rozdelena na vic bytu
        int length_of_length = static_cast<unsigned char>(message[i]) - 0x80;
        string divided_length = ""; //ziskame rozdelenou hodnotu
        for (int p = 0; p < length_of_length; ++p) 
        {
            i++;
            divided_length += message[i];
        }
        length = getlength(divided_length); //prevedeme na integer
        i += length;                        //preskocime dany pocet bytu
    }
    else 
    {
        i += message[i];                    //hodnota delky na jeden byte 
    }  

    return ++i;  
}

//dekoduje a vrati Search Response ve tvaru:
//kazdy vraceny zaznam na jednom radku
//jednotlive atributy zaznamu oddeleny znakem &
//nazev atributu a jeho hodnota oddeleny znakem =
//poradi atributu takove, jak to server poslal
//response_end - indikator, ze byla prijata cela odpoved serveru, coz nemusi byt
//ldap_error - indikator, ze server vratil chybu
string decode(string response, bool& response_end, bool& ldap_error) 
{
    int i = 0;
    int p;
    int length;
    int state = 0;
    string decoded = "";
    bool first = true;
    response_end = false; //indikator, ze byla prijata cela odpoved serveru, coz nemusi byt
    ldap_error = false;   //indikator, ze server vratil chybu
 
    //prochazime od zacatku
    while (i < response.size()) 
    { 
        switch (state) 
        {
            case 0: //sequence
                i = skip_length(response, i, length); //sequence
                if (response[i] == INTEGER) 
                {
                    state = 1; //id
                }
                else if (response[i] == STRING) 
                {
                    state = 3; //string
                }
                break;
            case 1: //id
                i = skip_tlv(response, i); //id
                if (response[i] == AP4) 
                    state = 2; //application 4, entry
                else if (response[i] == AP5)
                    state = 4; //application 5, resultCode
                else if (response[i] == AP1) 
                {
                    state = 4; //application 1, bindResponse
                }
                break;
            case 2: //application 4, entry
                decoded += '\n'; //odradkovani
                i = skip_length(response, i, length); //application 4 
                i = skip_tlv(response, i);            //objectname
                state = 0; //sequence
                break;
            case 3: //string
                i = skip_length(response, i, length); //string
                for (p = 0; p < length; ++p) 
                {
                    decoded += response[i];
                    i++;
                }  
                decoded += '='; 
                i = skip_length(response, i, length); //set of
                i = skip_length(response, i, length); //string
                for (p = 0; p < length; ++p) 
                {
                    decoded += response[i];
                    i++;
                }
                decoded += '&'; //oddeleni jednotlivych atributu
                state = 0; //sequence     
                break;
            case 4: //application 5, resultCode nebo bindResponse
                response_end = true; //jsme na konci odpovedi serveru, byla prijata cela
                i = skip_length(response, i, length); //application 5
                i = skip_length(response, i, length); //enumerated
                if (response[i] != 0) 
                {
                    if (response[i] == 4) 
                    {
                        cerr << "Error: " << "Vysledku vyhledavani je prilis mnoho, zkuste zuzit vyhledavaci kriteria." << endl;
                    }
                    else 
                    {
                        cerr << "Error " << static_cast<int>(response[i]) << "!" << endl;
                    }
                    ldap_error = true;
                }
                i++;
                i = skip_length(response, i, length); //string 
                if (length != 0) 
                {
                    cout << "MatchedDN: ";
                    for (p = 0; p < length; ++p) 
                    {
                        cout << static_cast<int>(response[i]) << endl;
                        i++;
                    }
                } 
                i = skip_length(response, i, length); //string 
                if (length != 0) 
                {
                    cerr << "Errormessage: ";
                    for (p = 0; p < length; ++p) 
                    {
                        cerr << response[i];
                        i++;
                    }
                    cerr << endl;
                }
                break;
        }   
    }
 
    return decoded;
}

//vytvori a vrati searchRequest
string createrequest(int id, string base, int depth, string filter, string result) 
{
    string searchrequest = "";
    int i;
    int j;

    //vlozi atributy z result
    i = result.size()-1;
    j = 0;
    while (i >= 0) 
    {
        if (result[i] != ' ') 
        {
            //atribut
            searchrequest.insert(searchrequest.begin(), result[i]);
            j++;
        }
        else 
        { 
            //jeho delka
            divide_length(searchrequest, j);
            searchrequest.insert(searchrequest.begin(), STRING); 
            j = 0;       
        }
        i--;
    }
    
    if (j != 0) 
    { 
        divide_length(searchrequest, j);
        searchrequest.insert(searchrequest.begin(), STRING);
    }
    
    divide_length(searchrequest, searchrequest.size());
    searchrequest.insert(searchrequest.begin(), SEQUENCE);    
      
    //vlozi filter
    searchrequest.insert(0, encode(filter));    
    
    //attrsonly
    searchrequest.insert(searchrequest.begin(), 0x00);
    searchrequest.insert(searchrequest.begin(), 0x01);
    searchrequest.insert(searchrequest.begin(), 0x01);

    //timelimit
    searchrequest.insert(searchrequest.begin(), 0x00);
    searchrequest.insert(searchrequest.begin(), 0x01);
    searchrequest.insert(searchrequest.begin(), INTEGER);

    //sizelimit 
    searchrequest.insert(searchrequest.begin(), 0x00);
    searchrequest.insert(searchrequest.begin(), 0x01);
    searchrequest.insert(searchrequest.begin(), INTEGER);

    //derefAliases
    searchrequest.insert(searchrequest.begin(), 0x00);
    searchrequest.insert(searchrequest.begin(), 0x01);
    searchrequest.insert(searchrequest.begin(), ENUMERATED);

    //scope
    searchrequest.insert(searchrequest.begin(), depth);
    searchrequest.insert(searchrequest.begin(), 0x01);
    searchrequest.insert(searchrequest.begin(), ENUMERATED);

    //baseObject
    searchrequest.insert(0, base);
    divide_length(searchrequest, base.size());
    searchrequest.insert(searchrequest.begin(), STRING);

    //searchRequest
    divide_length(searchrequest, searchrequest.size());
    searchrequest.insert(searchrequest.begin(), AP3); 

    //messageID
    if (id >= UCHAR_MAX) 
    {
        //id je na vic bytu
        string divided_id = divide_number(id);
        searchrequest.insert(0, divided_id);
        divide_length(searchrequest, divided_id.size());
    }
    else 
    {
        //id je na jeden byte
        searchrequest.insert(searchrequest.begin(), id);
        searchrequest.insert(searchrequest.begin(), 0x01);   
    }
    searchrequest.insert(searchrequest.begin(), INTEGER);

    //sequence
    divide_length(searchrequest, searchrequest.size());
    searchrequest.insert(searchrequest.begin(), SEQUENCE);

    return searchrequest;
}

//vytvori a vrati unbindRequest
string create_unbind(int id) 
{
    string unbindrequest;
    unbindrequest.insert(unbindrequest.begin(), 0x00); //0 bytu
    unbindrequest.insert(unbindrequest.begin(), 0x05); //NULL
    unbindrequest.insert(unbindrequest.begin(), 0x42); //APPLICATION 2
    string divided_id = divide_number(id);
    unbindrequest.insert(0, divided_id); //message id
    divide_length(unbindrequest, divided_id.size());
    unbindrequest.insert(unbindrequest.begin(), INTEGER);
    unbindrequest.insert(unbindrequest.begin(), unbindrequest.size());
    unbindrequest.insert(unbindrequest.begin(), SEQUENCE);

    return unbindrequest; 
}

//prevede dekodovanou odpoved do vysledneho formatu a vytiskne
//decoded - vystup funkce decode
//attributes - atributy, ktere se maji v danem poradi u kazde polozky vytisknout
void print_results(string decoded, string attributes) 
{
    int i = 0; //output
    int j = 0; //attributes
    int k = 0; //attribute
    int row = 0;
    int rows = 0;
    int countn = 0;
    int match = 0;
    string attribute = "";
    string output = "";
    int rowStart = 0;

    if (decoded.size() == 0) 
    {
        cout << "Nic nenalezeno." << endl;
        return;
    }

    cout << "Vysledky:" << endl;

    //pokud nejsou zadany atributy, vypisou se vsechny
    if (attributes == "") 
    {
        while (decoded[i] != '\0') 
        {
            if (decoded[i] == '=') 
            {
                cout << ": "; 
            }
            else if (decoded[i] == '&') 
            {
                cout << endl;
            }
            else 
            {
                cout << decoded[i];
            }
                   
            i++;
        }
        return;
    }

    //zjisti pocet radku dekodovane odpovedi
    while (i < decoded.size()) 
    {
         if (decoded[i] == '\n') rows++;
         i++;
    }

    for (row = 1; row <= rows; ++row) 
    {
        //zpracuje jeden radek
        j = 0;
        while (j < attributes.size()) 
        {
            k = 0;
            attribute = "";
            //ziska nazev atributu a posune index na zacatek dalsiho
            while (j != attributes.size() && attributes[j] != ' ') 
            { 
                attribute += attributes[j];
                j++;
            }  
            j++;
            //posune index na zacatek aktualniho radku 
            i = rowStart;
            while (countn != row) 
            {
                if (decoded[i] == '\n') countn++;
                i++;  
            }
            rowStart = i;
            //najde na radku shodu s nazvem atributu a posune index na zacatek hodnoty
            match = 0;
            while (i != decoded.size()-1 && decoded[i] != '\n') 
            {
                if (decoded[i] == attribute[k]) 
                {
                     match++;
                     if (match == attribute.size()) 
                     {
                         break;
                     }
                     k++;
                }
                else 
                {
                    match = 0;
                    k = 0;
                }
                i++;
            }
            i++;
            if (match != 0) 
            {
                i++; //preskoci =
                //nakopiruje hodnotu do vysledneho outputu
                while (decoded[i] != '&') 
                {
                    output += decoded[i];
                    i++;    
                }
            }
            //oddelovac
            if (j < attributes.size()) 
            {
                output += " | ";
            }
            else 
            {
                output += '\n';
            } 
        }
    }

    cout << output;      
}

//zjisti od uzivatele potrebny pocet retezcu a vlozi je do filteru na mista %s, vrati vysledny filter
string input(string filter) 
{
    string searchstring;
    int i; //pozice ve filteru
    int j = 0; //kolikaty retezec se od uzivatele chce

    while (1) 
    {
        j++;
        i = filter.find("%s", 0);
        if (i == string::npos) break; //kdyz v retezci neni zadne %s
        if (j == 1) cout << endl;
        cout << "Vlozte ";
        if (j > 1) cout << j << ". ";
        cout << "retezec: ";
        getline(cin, searchstring);
        filter.replace(i,2,searchstring); 
    }

    return filter;
}

//odesle socketem s zpravu message
//pri chybe vrati -1
int send(int s, string message) 
{
    if (write(s, message.c_str(), message.size()) < 0) 
    {
        perror("write"); 
        return -1;
    }

    return 0;
}

//prijme na socketu s zpravu a ulozi ji do retezce response
//pri chybe vrati -1
int receive(int s, string & response) 
{
    int size;
    response = "";
    char buffer[10];
    while ((size = read(s, buffer, sizeof(buffer))) != 0) 
    {
        if (size == -1) 
        {
            perror("read"); 
            return -1;
        }
        for (int i = 0; i < size; ++i) 
        {
            response += buffer[i];               
        }             
        if (size != sizeof(buffer)) break;
    }

    return 0;
}

//zavola se po prijeti signalu SIGINT (ctrl+c)
//odesle unbindRequest, zavre socket a ukonci program
void finish(int s) 
{
    id++;
    string message = create_unbind(id);
    if (send(s, message) == -1) exit(-1);

    //uzavreni spojeni
    if (close(s) < 0) 
    {
        perror("close");
        exit(-1);
    }
    cout << endl;
    exit(0);
}

int main (int argc, char *argv[])
{
    if (argc != 2) 
    {
        cerr << "Neni parametr!" << endl;
        return -1;
    }

    string host, base, filter, filter2, result, message, searchstring, output, response;
    int port, depth;
    struct sockaddr_in sin; 
    struct hostent *hptr;

    signal(SIGINT, finish);

    //zpracovani konfiguracniho souboru
    if (config(argv[1], host, port, base, depth, filter, result) == -1) 
    {
        cerr << "Chyba pri zpracovani souboru " << argv[1] << "!" << endl;
        return -1;
    }
    int s;
    //vytvori socket
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        perror("socket"); 
        return -1;
    }

    sin.sin_family = PF_INET;   //rodina protokolu
    sin.sin_port = htons(port); //cislo portu

    //zjistime info o vzdalenem pocitaci
    if ((hptr = gethostbyname(host.c_str())) == NULL) 
    {
        fprintf(stderr, "gethostname error\n");
        return -1;
    }

    //nastaveni IP adresy, ke ktere se pripojime
    memcpy(&sin.sin_addr, hptr->h_addr, hptr->h_length);

    //navazani spojeni
    if (connect (s, (struct sockaddr *)&sin, sizeof(sin)) < 0) 
    {
        perror("connect"); 
        return -1; 
    }
 
    id = 0;
    while (1) 
    {
        message = "";
        if (id == 0) 
        {
            //prvni zprava - bindrequest
            for (int p = 0; p < sizeof(BINDREQUEST); ++p) 
            {
                message.insert(message.end(), BINDREQUEST[p]);
            }
            id++;
        }
        else 
        {
            //vlozeni uzivatelskych vstupu do filteru a vytvoreni searchrequestu
            filter2 = input(filter); 
            message = createrequest(id, base, depth, filter2, result);
            id++;
        }   

        //poslani pozadavku
        if (send(s, message) == -1) return -1;
            
        //prijmuti a vytisknuti odpovedi
        output = "";
        bool response_end;       //indikuje, zda bylo od serveru prijato uz vsechno
        bool ldap_error = false; //indikuje, zda server vratil chybu
        while (1) 
        {
            if (receive(s, response) == -1) return -1;
            output += decode(response, response_end, ldap_error);
            if (response_end) break;
        }
        if (id != 1 && !ldap_error) print_results(output, result); //neni to bindresponse a nedoslo k chybe, tiskni
        if (filter == filter2) finish(s);                          //ve filteru nebyly casti vyzadujici uzivatelsky vstup, konec po prvnim behu  
       
    }

    return 0;
}
