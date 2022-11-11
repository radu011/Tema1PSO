Am inceput prin a citi cerinta si mi-am construit structura _so_file cu urmatorii membrii:

_fileDescriptor -> aici retin descriptorul/Handle
_openMode -> retin modul de deschidere ("r", "w", ...)
_openingFlags -> flag-urile pe care le-am setat la open
_buffer[] -> buffer-ul de 4096 de caractere 
_bufferLen -> lungimea buffer-ului
_posInBuffer -> pozitia 'cursorului' in buffer
_posInFile -> am crezut ca voi mai avea nevoie de un parametru pentru retinerea pozitiei atunci cand citesc un calup mai mare (am ajuns sa imi dau seama ca nu o mai folosesc)
_realPosInFile -> aceasta variabila ma ajuta sa retin pozitia reala, si cand fac seek sau vreo operatie de put/get
_endOfFile -> flag pe care il setez atunci cand ajung la finalul fisierului
_errorFlag -> flag pe care il setez atunci cand intalnesc o eroare
_lastOp -> poate fi READ_FLAG sau WRITE_FLAG incat sa stiu care a fost ultima operatie facuta pe fisier
_pid -> retin pid-ul pentru proces

Am considerat ca in problema data functia so_fflush() este una ce are o pondere mare deoarece aceasta imi goleste buffer-ul de valori care trebuie scrise in fisier sau cand este vorba de ultima operatie de citire, acesta sa goleasca buffer-ul. Aceasta functie este apelata de majoritatea functiilor de citire/scriere sau de inchidere a fisierului in functie de ultima operatie si ce se afla in buffer.

Pentru implementarea functiei so_fread si so_fwrite m-am folosit de functiile so_fgetc respectiv so_fputc, fiind mult mai usor de implementat acestea in acest mod. Au necesitat doar cateva verificari suplimentare pentru a fi sigur ca operatiile se executa corespunzator.

In rest, in implementarea functiilor am respectat cerintele prezente pe wiki.
Nu am reusit sa implementez functiile so_fopen si so_fclose.
*Consider ca timpul intre laboratoarele de procese\pipe-uri a fost destul de scurt pana la predarea temei si nu am reusit sa implementez exercitiile din laborator si apoi sa lucrez la aceste 2 functii.