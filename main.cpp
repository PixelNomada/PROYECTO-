#include <nds.h>
#include <fat.h>
#include <filesystem.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// DSi IA - version definitiva offline
// Objetivo: Nintendo DS/DSi + flashcart con DLDI/libfat.
// Todo funciona sin Internet. La memoria se guarda en DSiIA.dat.

#define INPUT_MAX 120
#define MSG_MAX 112
#define HISTORY_MAX 80
#define LEARN_MAX 300
#define ANSWER_MAX 1536
#define KNOWLEDGE_FILE "knowledge.txt"
#define DATA_FILE "DSiIA.dat"

struct Message { char who; char text[MSG_MAX]; };
struct Learned { char q[96]; char a[MSG_MAX]; };

static PrintConsole topScreen, bottomScreen;
static Message history[HISTORY_MAX];
static Learned learned[LEARN_MAX];
static int historyCount=0, learnedCount=0, scroll=0;
static char input[INPUT_MAX+1];
static int inputLen=0;
static bool storageOK=false;
static bool knowledgeOK=false;
static bool detailed=false;
static bool calcMode=false;
static bool saidMode=false;
static bool caps=false;

static void copyText(char* d,const char* s,int n){ strncpy(d,s,n-1); d[n-1]=0; }

static void normalize(const char* in,char* out,int n){
    int j=0;
    for(int i=0;in[i] && j<n-1;i++){
        unsigned char c=(unsigned char)in[i];
        if(c>='A'&&c<='Z') c=(unsigned char)(c+32);
        if(c=='?'||c=='!'||c=='.'||c==','||c==';'||c==':'||c=='('||c==')'||c=='['||c==']'||c=='"'||c=='\'') continue;
        if(c=='\t'||c=='\r'||c=='\n') c=' ';
        out[j++]=(char)c;
    }
    out[j]=0;
    while(j>0 && out[j-1]==' ') out[--j]=0;
}

static bool contains(const char* s,const char* q){ return strstr(s,q)!=0; }

static void addHistory(char who,const char* text){
    if(historyCount>=HISTORY_MAX){
        for(int i=1;i<HISTORY_MAX;i++) history[i-1]=history[i];
        historyCount=HISTORY_MAX-1;
    }
    history[historyCount].who=who;
    copyText(history[historyCount].text,text,MSG_MAX);
    historyCount++;
    if(scroll>0) scroll=0;
}

static void saveData(){
    if(!storageOK) return;
    FILE* f=fopen(DATA_FILE,"wb");
    if(!f) return;
    unsigned short magic=0xD51A, ver=2, hc=(unsigned short)historyCount, lc=(unsigned short)learnedCount;
    fwrite(&magic,2,1,f); fwrite(&ver,2,1,f); fwrite(&hc,2,1,f); fwrite(&lc,2,1,f);
    fwrite(history,sizeof(Message),historyCount,f);
    fwrite(learned,sizeof(Learned),learnedCount,f);
    fclose(f);
}

static void loadData(){
    if(!storageOK) return;
    FILE* f=fopen(DATA_FILE,"rb");
    if(!f) return;
    unsigned short magic=0,ver=0,hc=0,lc=0;
    if(fread(&magic,2,1,f)!=1 || fread(&ver,2,1,f)!=1 || fread(&hc,2,1,f)!=1 || fread(&lc,2,1,f)!=1){ fclose(f); return; }
    if(magic!=0xD51A || ver!=2){ fclose(f); return; }
    if(hc>HISTORY_MAX) hc=HISTORY_MAX; if(lc>LEARN_MAX) lc=LEARN_MAX;
    historyCount=(int)hc; learnedCount=(int)lc;
    fread(history,sizeof(Message),historyCount,f);
    fread(learned,sizeof(Learned),learnedCount,f);
    fclose(f);
}

static void addLearned(const char* q,const char* a){
    if(learnedCount>=LEARN_MAX){
        for(int i=1;i<LEARN_MAX;i++) learned[i-1]=learned[i];
        learnedCount=LEARN_MAX-1;
    }
    copyText(learned[learnedCount].q,q,96);
    copyText(learned[learnedCount].a,a,MSG_MAX);
    learnedCount++;
    saveData();
}

static const char* lookupLearned(const char* q){
    for(int i=learnedCount-1;i>=0;i--) if(strcmp(q,learned[i].q)==0) return learned[i].a;
    return 0;
}

struct KB { const char* key; const char* answer; };

static const KB kb[] = {
{"fraccion","Una fraccion representa partes de un todo: numerador arriba y denominador abajo. Ejemplo: 3/4."},
{"suma de fracciones","Con igual denominador suma numeradores. Con distinto denominador busca un denominador comun y luego suma."},
{"ecuacion lineal","En ax+b=c, despeja: x=(c-b)/a, siempre que a no sea 0."},
{"ecuacion cuadratica","Una cuadratica tiene forma ax^2+bx+c=0. Sus soluciones usan el discriminante b^2-4ac y la formula cuadratica."},
{"formula cuadratica","x=(-b +- raiz(b^2-4ac))/(2a). Si el discriminante es positivo hay dos raices reales; cero da una; negativo da soluciones complejas."},
{"potencia","a^n significa multiplicar a por si misma n veces. Reglas: a^m*a^n=a^(m+n) y (a^m)^n=a^(mn)."},
{"raiz cuadrada","La raiz cuadrada de n es el numero que al multiplicarse por si mismo da n. Ejemplo: raiz de 81 = 9."},
{"porcentaje","x por ciento significa x/100. Para hallar 20% de 150: 150*0.20=30."},
{"regla de tres","Si a corresponde a b y c corresponde a x, entonces x=(b*c)/a cuando la relacion es directamente proporcional."},
{"pitagoras","En un triangulo rectangulo: a^2+b^2=c^2, donde c es la hipotenusa."},
{"pendiente","La pendiente entre dos puntos es m=(y2-y1)/(x2-x1). Mide cuanto cambia y por cada unidad de x."},
{"funcion","Una funcion relaciona cada valor de entrada con un unico valor de salida. En y=mx+b, m es pendiente y b es interseccion."},
{"probabilidad","Probabilidad = casos favorables/casos posibles cuando todos los resultados son equiprobables."},
{"area triangulo","Area = base*altura/2."},
{"area circulo","Area = pi*r^2. La circunferencia es 2*pi*r."},
{"perimetro","El perimetro es la suma de las longitudes de todos los lados de una figura."},
{"volumen cilindro","Volumen = pi*r^2*h."},
{"logaritmo","log_b(x)=y significa b^y=x. El logaritmo convierte potencias en multiplicaciones de exponentes."},
{"derivada","La derivada mide la tasa de cambio instantanea. Para x^n, la derivada es n*x^(n-1)."},
{"media aritmetica","La media es la suma de los datos dividida entre la cantidad de datos."},
{"mediana","Ordena los datos. Si hay cantidad impar toma el centro; si es par promedia los dos centrales."},
{"quimica","La quimica estudia la materia, sus propiedades, composicion y transformaciones."},
{"atomo","Un atomo tiene nucleo con protones y neutrones, y electrones alrededor. El numero de protones define el elemento."},
{"tabla periodica","Organiza los elementos por numero atomico y propiedades. Las columnas son grupos y las filas periodos."},
{"numero atomico","Es el numero de protones del nucleo. Se representa con Z."},
{"masa atomica","Es la masa promedio de los atomos de un elemento considerando sus isotopos y abundancias."},
{"isotopo","Son atomos del mismo elemento con igual numero de protones pero diferente numero de neutrones."},
{"ion","Un ion es un atomo o grupo con carga electrica porque gano o perdio electrones."},
{"enlace ionico","Se forma normalmente entre un metal y un no metal mediante transferencia de electrones y atraccion de iones."},
{"enlace covalente","Se forma cuando atomos comparten pares de electrones, comunmente entre no metales."},
{"enlace metalico","En los metales, los atomos comparten electrones deslocalizados, lo que ayuda a explicar conductividad y maleabilidad."},
{"ph","El pH mide la acidez o basicidad. Menor que 7 es acido, 7 es neutro y mayor que 7 es basico a 25 C."},
{"mol","Un mol contiene aproximadamente 6.022e23 entidades. Es una unidad para contar particulas a escala microscopica."},
{"masa molar","Es la masa de un mol de sustancia, expresada normalmente en g/mol."},
{"oxidacion","La oxidacion implica perdida de electrones. La reduccion implica ganancia de electrones; juntas forman reacciones redox."},
{"balancear ecuaciones","Conserva el numero de atomos de cada elemento. Cambia coeficientes delante de formulas, no los subindices."},
{"reaccion quimica","Es un proceso donde unas sustancias se transforman en otras por reorganizacion de atomos y enlaces."},
{"fisica","La fisica estudia materia, energia, movimiento, fuerzas, espacio y tiempo."},
{"velocidad","Velocidad media = distancia/tiempo. Si importa direccion hablamos de velocidad vectorial."},
{"aceleracion","a=(vf-vi)/t. Indica como cambia la velocidad con el tiempo."},
{"segunda ley de newton","F=m*a. La fuerza neta sobre un objeto es igual a su masa por su aceleracion."},
{"primera ley de newton","Un objeto mantiene reposo o movimiento rectilineo uniforme si la fuerza neta es cero."},
{"tercera ley de newton","Las fuerzas aparecen en pares: si A ejerce fuerza sobre B, B ejerce una fuerza igual y opuesta sobre A."},
{"energia cinetica","Ec=1/2*m*v^2. Es energia asociada al movimiento."},
{"energia potencial","Cerca de la superficie terrestre: Ep=m*g*h."},
{"trabajo","Trabajo mecanico W=F*d*cos(theta) cuando una fuerza desplaza un objeto."},
{"potencia fisica","Potencia es rapidez de transferencia de energia. P=W/t."},
{"densidad","Densidad = masa/volumen."},
{"presion","Presion = fuerza/area. En un fluido en reposo tambien depende de densidad, gravedad y profundidad."},
{"ley de ohm","V=I*R. V es voltaje, I corriente y R resistencia."},
{"potencia electrica","P=V*I. Tambien P=I^2*R o P=V^2/R cuando corresponde."},
{"biologia","La biologia estudia los seres vivos, sus estructuras, funciones, evolucion y relaciones con el ambiente."},
{"celula","La celula es la unidad basica de los seres vivos. Procariotas no tienen nucleo rodeado por membrana; eucariotas si."},
{"mitocondria","Organelo relacionado con la respiracion celular y la produccion de ATP en celulas eucariotas."},
{"fotosintesis","Las plantas usan luz para transformar agua y CO2 en materia organica, liberando oxigeno como producto de la fotosintesis oxigenica."},
{"adn","El ADN almacena informacion genetica. Sus bases son adenina, timina, citosina y guanina."},
{"ecosistema","Incluye seres vivos y factores no vivos que interactuan en un lugar."},
{"evolucion","Es el cambio de las poblaciones a traves de generaciones. La seleccion natural es uno de sus mecanismos."},
{"sistema nervioso","Coordina respuestas mediante neuronas y otras celulas. Incluye sistema nervioso central y periferico."},
{"gen","Un gen es una region de ADN que participa en la produccion de un producto funcional o en la regulacion de una funcion biologica."},
{"espanol","El espanol es una lengua romance derivada principalmente del latin vulgar y tiene variacion regional."},
{"sustantivo","Nombra personas, animales, lugares, objetos, ideas o conceptos. Ejemplos: casa, Mexico, libertad."},
{"verbo","Expresa acciones, procesos o estados. Ejemplos: correr, pensar, ser."},
{"adjetivo","Describe o especifica caracteristicas de un sustantivo: rapido, azul, enorme."},
{"adverbio","Modifica un verbo, adjetivo u otro adverbio. Ejemplos: rapidamente, muy, ayer."},
{"metafora","Relaciona dos ideas de forma figurada sin usar necesariamente como: tus ojos son estrellas."},
{"simil","Compara explicitamente dos elementos usando palabras como como o parece."},
{"oracion","Una oracion es una unidad con sentido completo. Muchas oraciones contienen sujeto y predicado."},
{"sujeto y predicado","El sujeto indica de quien se habla; el predicado aporta lo que se dice sobre ese sujeto."},
{"acentuacion","Las palabras agudas se acentuan segun las reglas de terminacion; graves y esdrujulas siguen reglas diferentes. Las esdrujulas siempre llevan tilde."},
{"ingles","El ingles usa estructuras como sujeto + verbo + complemento. El orden importa mucho para formar oraciones claras."},
{"present simple","Se usa para rutinas, hechos y situaciones habituales. Con he, she, it el verbo suele llevar -s."},
{"past simple","Se usa para acciones terminadas en el pasado. Verbos regulares suelen usar -ed; muchos irregulares tienen formas propias."},
{"present continuous","Se forma con am/is/are + verbo terminado en -ing para acciones en progreso."},
{"verbo to be","Sus formas principales en presente son am, is y are. En pasado son was y were."},
{"there is","There is se usa para indicar que hay una cosa o entidad en singular."},
{"there are","There are se usa para indicar que hay varias cosas o entidades."},
{"pronombres ingles","I, you, he, she, it, we, you, they son pronombres personales de sujeto."},
{"independencia de mexico","La lucha iniciada en 1810 termino politicamente en 1821 con la consumacion de la Independencia. Participaron movimientos y proyectos politicos distintos."},
{"miguel hidalgo","Miguel Hidalgo y Costilla inicio el movimiento insurgente de 1810 con el llamado conocido como Grito de Dolores."},
{"morelos","Jose Maria Morelos dirigio una etapa importante de la insurgencia y presento los Sentimientos de la Nacion en 1813."},
{"iturbide","Agustin de Iturbide impulso el Plan de Iguala en 1821 y entro en Mexico con el Ejercito Trigarante; ese proceso llevo a la consumacion de la Independencia."},
{"revolucion mexicana","Fue un proceso politico y armado iniciado en 1910 contra el regimen de Porfirio Diaz y que tuvo distintas facciones y objetivos."},
{"constitucion de 1917","La Constitucion mexicana de 1917 establecio derechos sociales y una nueva organizacion juridica del Estado; sigue siendo la constitucion vigente de Mexico, con reformas."},
{"revolucion francesa","Comenzo en 1789 y transformo la estructura politica y social de Francia. La Declaracion de los Derechos del Hombre y del Ciudadano fue un documento importante."},
{"democracia","Es un sistema politico en el que la autoridad publica se legitima mediante la participacion ciudadana y reglas institucionales."},
{"derechos humanos","Son derechos inherentes a todas las personas. Los Estados tienen obligaciones de respetarlos, protegerlos y garantizarlos conforme al marco juridico aplicable."},
{"latitud","La latitud mide la distancia angular al norte o sur del ecuador."},
{"longitud","La longitud mide la distancia angular al este u oeste del meridiano de Greenwich."},
{"clima","Describe patrones atmosfericos de largo plazo, mientras que el tiempo atmosferico describe condiciones de corto plazo."},
{"placas tectonicas","La litosfera esta dividida en placas que se mueven lentamente. Sus interacciones producen terremotos, volcanismo y montanas."},
{"ciclo del agua","Incluye evaporacion, condensacion, precipitacion, infiltracion, escorrentia y otros intercambios de agua."},
{"arte","El arte incluye practicas creativas como pintura, escultura, musica, literatura, danza, teatro y artes digitales."},
{"perspectiva","En dibujo, la perspectiva crea una sensacion de profundidad mediante tamaño, posicion y lineas que convergen hacia puntos de fuga."},
{"color","Los colores pueden describirse mediante matiz, saturacion y valor. La mezcla depende del medio: luz y pigmento no funcionan igual."}
};
static const int KB_COUNT=sizeof(kb)/sizeof(kb[0]);

// Base de conocimientos ampliada: conceptos generales y respuestas de identidad.
static const KB extraKB[] = {
{"que es un perro","Un perro es un mamifero domestico de la familia Canidae. Su nombre cientifico es Canis lupus familiaris. Tiene gran variedad de tamaños y razas, y puede aprender conductas mediante experiencia y entrenamiento."},
{"perro","Un perro es un mamifero domestico de la familia de los canidos. Se caracteriza por su gran capacidad de aprendizaje, comunicacion y adaptacion a distintos entornos."},
{"que es el planeta tierra","La Tierra es el tercer planeta del Sistema Solar desde el Sol. Es un planeta rocoso con atmosfera, agua liquida en su superficie y una enorme diversidad de vida conocida."},
{"planeta tierra","La Tierra tiene una estructura interna formada, de manera simplificada, por corteza, manto y nucleo. Su atmosfera esta compuesta principalmente por nitrogeno y oxigeno."},
{"que es un pais","Un pais es un territorio organizado politicamente bajo una autoridad y un marco juridico. Un Estado suele incluir territorio, poblacion, instituciones de gobierno y soberania."},
{"que es un arbol","Un arbol es una planta generalmente leñosa y perenne con un tallo principal llamado tronco. Sus hojas realizan fotosintesis y sus raices ayudan a absorber agua y minerales."},
{"arbol","Los arboles son plantas vasculares. Transportan agua y minerales desde las raices y distribuyen sustancias organicas producidas principalmente en las hojas."},
{"que es una planta","Una planta es un organismo del reino Plantae. Muchas producen materia organica mediante fotosintesis y tienen estructuras como raices, tallos y hojas."},
{"que es un animal","Un animal es un organismo del reino Animalia. En general es pluricelular, obtiene energia consumiendo materia organica y presenta capacidad de respuesta al ambiente."},
{"que es el sol","El Sol es una estrella situada en el centro del Sistema Solar. Es una esfera de plasma cuya energia procede principalmente de la fusion nuclear de hidrogeno en su nucleo."},
{"que es la luna","La Luna es el satelite natural de la Tierra. Sus fases aparentes se deben a la geometria cambiante entre el Sol, la Tierra y la Luna."},
{"que es una estrella","Una estrella es un objeto astronomico que mantiene reacciones de fusion nuclear en su interior durante una parte de su vida y emite energia al espacio."},
{"que es una galaxia","Una galaxia es un sistema enorme de estrellas, gas, polvo, materia oscura y otros componentes unidos principalmente por gravedad. La Via Lactea es nuestra galaxia."},
{"que es el sistema solar","El Sistema Solar esta formado por el Sol y todos los objetos que orbitan a su alrededor, incluidos planetas, planetas enanos, asteroides y cometas."},
{"que es el agua","El agua es un compuesto quimico formado por hidrogeno y oxigeno, H2O. En la Tierra aparece de forma natural como solido, liquido y gas."},
{"que es el aire","El aire es la mezcla de gases de la atmosfera terrestre. Cerca de la superficie contiene principalmente nitrogeno y oxigeno, ademas de cantidades menores de otros gases y vapor de agua."},
{"que es el oxigeno","El oxigeno es el elemento quimico de numero atomico 8. El gas O2 es importante para la respiracion de muchos organismos y participa en numerosas reacciones quimicas."},
{"que es el carbono","El carbono es el elemento de numero atomico 6. Puede formar una gran variedad de enlaces y es fundamental para la quimica organica y la vida conocida."},
{"que es una molecula","Una molecula es una entidad formada por dos o mas atomos unidos mediante enlaces quimicos, con una estructura y composicion definidas."},
{"que es un elemento quimico","Un elemento quimico es una sustancia cuyos atomos tienen el mismo numero de protones. Cada elemento se identifica por su numero atomico."},
{"que es la materia","La materia es todo aquello que tiene masa y ocupa espacio. Puede presentarse en diferentes estados y experimentar cambios fisicos y quimicos."},
{"estado solido","En un solido, las particulas estan relativamente proximas y el material mantiene una forma y volumen definidos bajo condiciones determinadas."},
{"estado liquido","Un liquido conserva su volumen aproximadamente, pero adopta la forma del recipiente porque sus particulas pueden desplazarse unas respecto de otras."},
{"estado gaseoso","Un gas no mantiene una forma ni un volumen propios y tiende a ocupar el espacio disponible. Sus particulas estan muy separadas comparadas con un solido."},
{"cambio fisico","Un cambio fisico modifica el estado, forma o apariencia de una sustancia sin cambiar su identidad quimica, como derretir hielo."},
{"cambio quimico","Un cambio quimico produce sustancias nuevas con composiciones y propiedades diferentes, como ocurre en una reaccion quimica."},
{"que es la energia","La energia es una magnitud asociada con la capacidad de producir cambios o realizar trabajo. Se presenta en formas como cinetica, potencial, termica, quimica y electrica."},
{"que es una fuerza","Una fuerza es una interaccion capaz de cambiar el movimiento de un objeto o producir una deformacion. Se mide en newtons."},
{"gravedad","La gravedad es la interaccion asociada a la masa. Cerca de la superficie terrestre produce una aceleracion aproximada de 9.8 m/s^2 hacia el centro de la Tierra."},
{"masa","La masa mide la cantidad de materia de un objeto y tambien su inercia. En el Sistema Internacional se expresa en kilogramos."},
{"peso","El peso es una fuerza gravitatoria. Cerca de la superficie terrestre puede calcularse aproximadamente como P=m*g."},
{"inercia","La inercia es la tendencia de un cuerpo a mantener su estado de movimiento cuando la fuerza neta sobre el no cambia ese estado."},
{"temperatura","La temperatura describe el estado termico de un sistema y se relaciona con la energia cinetica promedio de sus particulas en un modelo microscopico."},
{"calor","El calor es energia que se transfiere entre sistemas debido a una diferencia de temperatura."},
{"electricidad","La electricidad estudia fenomenos relacionados con cargas electricas, campos, corrientes y potenciales."},
{"carga electrica","La carga electrica es una propiedad fisica responsable de interacciones electromagneticas. Puede ser positiva o negativa y se mide en coulombs."},
{"campo electrico","Un campo electrico describe la influencia que una carga produciria sobre otra carga de prueba en diferentes posiciones."},
{"onda","Una onda es una perturbacion que se propaga y transporta energia e informacion sin requerir que la materia viaje globalmente con ella."},
{"luz","La luz es radiacion electromagnetica visible para el ojo humano. En el vacio se propaga a unos 299792458 m/s."},
{"sonido","El sonido es una onda mecanica que necesita un medio material para propagarse. Su velocidad depende del medio y de sus condiciones."},
{"que es una celula","La celula es la unidad estructural y funcional basica de los seres vivos. Las celulas procariotas carecen de nucleo membranoso y las eucariotas lo poseen."},
{"nucleo celular","El nucleo de una celula eucariota contiene la mayor parte del ADN y participa en la regulacion de la expresion genetica."},
{"ribosoma","El ribosoma es una estructura celular que participa en la sintesis de proteinas a partir de la informacion del ARN mensajero."},
{"cloroplasto","El cloroplasto es un organelo de las plantas y algas fotosinteticas donde ocurren las principales etapas de la fotosintesis."},
{"respiracion celular","La respiracion celular es el conjunto de procesos mediante los cuales las celulas obtienen energia util a partir de moleculas organicas. En muchos organismos participa el oxigeno."},
{"cadena alimentaria","Una cadena alimentaria representa relaciones de alimentacion y transferencia de energia entre organismos de un ecosistema."},
{"productor biologico","Un productor es un organismo que fabrica materia organica a partir de sustancias inorganicas, por ejemplo mediante fotosintesis o quimiosintesis."},
{"consumidor biologico","Un consumidor obtiene materia y energia alimentandose de otros organismos o de materia organica."},
{"descomponedor","Los descomponedores transforman materia organica muerta y contribuyen al reciclaje de nutrientes en los ecosistemas."},
{"biodiversidad","La biodiversidad es la variedad de vida en diferentes niveles, incluyendo diversidad genetica, de especies y de ecosistemas."},
{"adaptacion biologica","Una adaptacion es una caracteristica heredable que puede favorecer la supervivencia o reproduccion en un ambiente determinado a traves de generaciones."},
{"seleccion natural","La seleccion natural ocurre cuando las diferencias heredables entre individuos producen diferencias en supervivencia o reproduccion y cambian la frecuencia de rasgos en poblaciones."},
{"mutacion","Una mutacion es un cambio en la secuencia del material genetico. Sus efectos pueden ser neutros, perjudiciales o beneficiosos dependiendo del contexto."},
{"que es el adn","El ADN es una molecula que almacena informacion genetica. En muchas celulas esta organizado en cromosomas y puede copiarse antes de la division celular."},
{"cromosoma","Un cromosoma es una estructura de ADN asociado con proteinas que organiza material genetico. El numero de cromosomas varia entre especies."},
{"ecosistema","Un ecosistema esta formado por organismos y factores no vivos que interactuan mediante flujos de energia y ciclos de materia."},
{"bioma","Un bioma es una gran region ecologica caracterizada por condiciones climaticas y comunidades biologicas dominantes, como desiertos o bosques tropicales."},
{"que es una fraccion","Una fraccion representa una cantidad dividida en partes. El numerador indica cuantas partes se consideran y el denominador en cuantas partes iguales se divide el todo."},
{"numeros enteros","Los numeros enteros incluyen negativos, cero y positivos, sin parte decimal: ..., -2, -1, 0, 1, 2, ..."},
{"numeros racionales","Los numeros racionales pueden escribirse como una fraccion de dos enteros con denominador distinto de cero. Incluyen enteros y decimales periodicos o exactos."},
{"numeros irracionales","Los numeros irracionales no pueden expresarse como una fraccion exacta de enteros. Su expansion decimal es infinita y no periodica, como raiz de 2."},
{"numero primo","Un numero primo es un entero mayor que 1 que tiene exactamente dos divisores positivos: 1 y el propio numero."},
{"maximo comun divisor","El maximo comun divisor es el mayor entero positivo que divide exactamente a dos o mas numeros."},
{"minimo comun multiplo","El minimo comun multiplo es el menor entero positivo que es multiplo comun de dos o mas numeros no nulos."},
{"notacion cientifica","La notacion cientifica escribe un numero como a por 10^n, donde 1 <= |a| < 10. Es util para cantidades muy grandes o pequeñas."},
{"valor absoluto","El valor absoluto representa la distancia de un numero respecto de cero. Siempre es no negativo."},
{"sistema de ecuaciones","Un sistema de ecuaciones contiene varias ecuaciones con las mismas variables. Resolverlo significa encontrar los valores que satisfacen todas simultaneamente."},
{"ecuacion lineal","Una ecuacion lineal tiene variables de primer grado. Su grafica en dos variables es una recta cuando no hay restricciones adicionales."},
{"polinomio","Un polinomio es una expresion algebraica formada por sumas y restas de terminos con exponentes enteros no negativos."},
{"monomio","Un monomio es un termino algebraico como 5x^2 o -3ab."},
{"binomio","Un binomio es una expresion algebraica con dos terminos, como x+4."},
{"teorema de pitagoras","En todo triangulo rectangulo, la suma de los cuadrados de los catetos es igual al cuadrado de la hipotenusa."},
{"semejanza","Dos figuras son semejantes cuando tienen la misma forma: sus angulos correspondientes son iguales y sus lados correspondientes guardan una proporcion constante."},
{"congruencia geometrica","Dos figuras son congruentes si tienen la misma forma y tamaño; una puede coincidir con la otra mediante movimientos rigidos."},
{"media","La media aritmetica se obtiene sumando los valores y dividiendo entre el numero de datos."},
{"moda","La moda es el valor que aparece con mayor frecuencia en un conjunto de datos. Puede haber mas de una moda o ninguna clara."},
{"varianza","La varianza mide la dispersion de los datos respecto de su media mediante el promedio de los cuadrados de las desviaciones."},
{"desviacion estandar","La desviacion estandar es la raiz cuadrada de la varianza y expresa la dispersion en las mismas unidades de los datos."},
{"ingles basico","Para formar muchas oraciones en ingles se usa sujeto + verbo + complemento. El contexto determina el tiempo verbal y la estructura."},
{"presente simple ingles","El presente simple se usa para rutinas, hechos y estados habituales. Con he, she e it normalmente se agrega s al verbo en afirmativo."},
{"pasado simple ingles","El pasado simple describe acciones terminadas. Los verbos regulares suelen terminar en -ed, mientras muchos irregulares tienen formas propias."},
{"futuro will","Will se usa para hablar de decisiones, predicciones y hechos futuros. La estructura basica es sujeto + will + verbo base."},
{"can ingles","Can expresa habilidad, posibilidad o permiso segun el contexto. Despues de can se usa el verbo en forma base."},
{"must ingles","Must expresa obligacion fuerte o necesidad segun el contexto. Despues de must se usa el verbo base."},
{"comparativos ingles","Los comparativos comparan dos elementos. Se usan estructuras como taller than o more interesting than segun la palabra."},
{"superlativos ingles","Los superlativos expresan el grado maximo dentro de un grupo, con formas como the tallest o the most interesting."},
{"articulo a ingles","A se usa normalmente delante de un sustantivo singular contable cuyo sonido inicial es consonantico."},
{"articulo an ingles","An se usa normalmente delante de un sustantivo singular contable cuyo sonido inicial es vocalico."},
{"articulo the ingles","The se usa para referirse a algo especifico o identificable para quienes hablan."},
{"que es un sustantivo","Un sustantivo es una palabra que puede nombrar personas, animales, lugares, objetos, conceptos o ideas."},
{"que es un verbo","Un verbo expresa una accion, proceso, estado o relacion dentro de una oracion."},
{"que es un adjetivo","Un adjetivo modifica un sustantivo y aporta caracteristicas o propiedades, como rapido, azul o enorme."},
{"que es un pronombre","Un pronombre puede sustituir o representar un sustantivo o una expresion nominal, segun la estructura de la oracion."},
{"que es una metafora","Una metafora relaciona dos realidades de forma figurada para producir un significado expresivo, sin afirmar que sean literalmente identicas."},
{"que es una hiperbòle","La hiperbole es una exageracion intencional usada para enfatizar una idea. En textos escolares conviene reconocerla como recurso literario, no como dato literal."},
{"que es una fabula","Una fabula es un relato breve que suele presentar una enseñanza o moraleja y puede utilizar animales con caracteristicas humanas."},
{"que es un mito","Un mito es un relato tradicional relacionado con personajes, fuerzas o acontecimientos extraordinarios y con frecuencia explica aspectos culturales o del mundo."},
{"que es una novela","Una novela es una obra narrativa extensa, generalmente en prosa, con personajes, acciones, espacios y una estructura temporal."},
{"que es una biografia","Una biografia narra la vida de una persona y puede incluir hechos, contexto, obras y acontecimientos relevantes."},
{"que es una fuente historica","Una fuente historica es un testimonio o vestigio utilizado para estudiar el pasado, como documentos, objetos, edificios, imagenes o testimonios."},
{"causas independencia mexico","La Independencia de Mexico tuvo multiples causas: desigualdades sociales, cambios politicos en la monarquia espanola, ideas ilustradas, crisis de legitimidad de 1808 y conflictos entre grupos de la sociedad novohispana."},
{"virreinato de nueva espana","Nueva Espana fue un territorio de la monarquia espanola en America, establecido tras la conquista y organizado como virreinato desde 1535 hasta la consumacion de la Independencia."},
{"porfiriato","El Porfiriato es el periodo asociado al largo dominio politico de Porfirio Diaz, caracterizado por crecimiento economico y modernizacion junto con fuertes desigualdades, conflictos politicos y concentracion del poder."},
{"plan de san luis","El Plan de San Luis, proclamado por Francisco I. Madero en 1910, desconocio la continuidad de Porfirio Diaz y llamo a levantarse en armas el 20 de noviembre."},
{"zapata","Emiliano Zapata fue un dirigente revolucionario asociado con la defensa de demandas agrarias y el Plan de Ayala."},
{"pancho villa","Francisco Villa, conocido como Pancho Villa, fue uno de los principales lideres militares de la Revolucion Mexicana y dirigio la Division del Norte."},
{"que es un estado","En politica, un Estado es una organizacion institucional que ejerce autoridad sobre una poblacion y un territorio mediante un orden juridico. No es exactamente lo mismo que gobierno."},
{"que es gobierno","El gobierno es el conjunto de autoridades e instituciones que ejercen funciones de direccion y administracion del Estado durante un periodo determinado."},
{"que es una constitucion","Una constitucion es la norma juridica fundamental que organiza el poder publico y reconoce derechos y principios basicos de un Estado."},
{"que es geografia","La geografia estudia el espacio geografico, los lugares, paisajes, sociedades y relaciones entre procesos naturales y humanos."},
{"relieve","El relieve es el conjunto de formas de la superficie terrestre, como montanas, llanuras, mesetas, valles y depresiones."},
{"rio","Un rio es una corriente natural de agua que fluye por un cauce hacia otro cuerpo de agua o hacia una zona de drenaje."},
{"oceano","Un oceano es una gran masa continua de agua salada que cubre gran parte de la superficie terrestre. Tradicionalmente se distinguen varios oceanos."},
{"desierto","Un desierto es una region que recibe muy poca precipitacion en relacion con la demanda evaporativa y puede presentar ambientes muy diversos."},
{"selva tropical","Una selva tropical es un ecosistema calido y generalmente humedo con gran diversidad biologica y vegetacion abundante."},
{"ciclo del carbono","El ciclo del carbono describe el intercambio de carbono entre atmosfera, oceanos, seres vivos, suelos, rocas y otros reservorios mediante procesos biologicos, quimicos y geologicos."},
{"efecto invernadero","El efecto invernadero es un proceso natural por el que ciertos gases atmosfericos absorben y reemiten radiacion infrarroja, ayudando a mantener la temperatura de la superficie. Su intensificacion por actividades humanas causa calentamiento global."},
{"cambio climatico","El cambio climatico es una alteracion sostenida de los patrones climaticos. Actualmente, el calentamiento observado se debe principalmente al aumento antropogenico de gases de efecto invernadero."},
{"placa tectonica","Una placa tectonica es una porcion rigida de la litosfera que se mueve lentamente sobre materiales mas ductiles del interior terrestre."},
{"terremoto","Un terremoto es una sacudida del terreno causada por una liberacion repentina de energia, frecuentemente asociada al movimiento de fallas tectonicas."},
{"volcan","Un volcan es una estructura geologica por la que magma, gases y otros materiales pueden llegar a la superficie o cerca de ella."},
{"que es el arte","El arte comprende formas de creacion y expresion humana como pintura, escultura, musica, literatura, danza, teatro, fotografia y medios digitales."},
{"pintura","La pintura es una disciplina visual que utiliza pigmentos o materiales equivalentes aplicados sobre una superficie para crear imagenes, formas o composiciones."},
{"escultura","La escultura crea formas tridimensionales mediante materiales como piedra, madera, metal, arcilla u otros medios."},
{"perspectiva artistica","La perspectiva es un conjunto de recursos para representar profundidad y espacio tridimensional sobre una superficie bidimensional."},
{"que es musica","La musica organiza sonidos y silencios mediante elementos como ritmo, melodia, armonia, timbre y dinamica."},
{"ritmo","El ritmo organiza duraciones y acentos en el tiempo y es un componente fundamental de la musica y otras artes escenicas."},
{"que es programacion","La programacion consiste en escribir instrucciones que una computadora puede ejecutar para realizar tareas, procesar datos o controlar sistemas."},
{"que es un algoritmo","Un algoritmo es una secuencia finita y ordenada de pasos para resolver un problema o realizar una tarea."},
{"que es internet","Internet es una red mundial de redes interconectadas que utiliza protocolos de comunicacion para intercambiar datos entre dispositivos y servicios."},
{"que es una computadora","Una computadora es un sistema electronico capaz de procesar datos mediante instrucciones almacenadas y producir resultados."},
{"que es inteligencia artificial","La inteligencia artificial es un campo de la informatica que desarrolla sistemas capaces de realizar tareas que normalmente requieren capacidades como reconocimiento de patrones, razonamiento o generacion de contenido."},
{"identidad dsi ia","Mi nombre es DSi IA. Soy un asistente educativo offline pensado para Nintendo DS/DSi. No soy un modelo gigante conectado a Internet: mi conocimiento esta almacenado dentro del programa y puedo aprender respuestas que tu me enseñes."},
{"personalidad dsi ia","Tengo una personalidad de asistente escolar curioso, directo y algo bromista. Puedo responder corto o entrar en Modo INTELIGENTE para explicar con mas detalle."},
{"como aprendes","Aprendo mediante el comando APRENDER pregunta | respuesta. La respuesta queda almacenada en la memoria de la consola y se vuelve a buscar antes de mi base general."},
{"como guardas","Guardo automaticamente la conversacion y lo aprendido en DSiIA.dat cuando el almacenamiento compatible esta disponible. Tambien puedes usar GUARDAR manualmente."}
};
static const int EXTRA_COUNT=sizeof(extraKB)/sizeof(extraKB[0]);

struct Word { const char* en; const char* es; };
static const Word dictionary[] = {
{"hello","hola"},{"hi","hola"},{"goodbye","adios"},{"please","por favor"},{"thanks","gracias"},{"thank you","gracias"},{"yes","si"},{"no","no"},{"friend","amigo"},{"family","familia"},{"house","casa"},{"home","hogar"},{"school","escuela"},{"teacher","maestro"},{"student","estudiante"},{"book","libro"},{"water","agua"},{"food","comida"},{"dog","perro"},{"cat","gato"},{"tree","arbol"},{"earth","tierra"},{"country","pais"},{"city","ciudad"},{"world","mundo"},{"sun","sol"},{"moon","luna"},{"star","estrella"},{"sky","cielo"},{"sea","mar"},{"river","rio"},{"mountain","montana"},{"school","escuela"},{"computer","computadora"},{"phone","telefono"},{"game","juego"},{"music","musica"},{"movie","pelicula"},{"car","auto"},{"road","camino"},{"time","tiempo"},{"day","dia"},{"night","noche"},{"morning","manana"},{"afternoon","tarde"},{"today","hoy"},{"tomorrow","manana"},{"yesterday","ayer"},{"now","ahora"},{"where","donde"},{"what","que"},{"who","quien"},{"when","cuando"},{"why","por que"},{"how","como"},{"because","porque"},{"and","y"},{"or","o"},{"but","pero"},{"with","con"},{"without","sin"},{"from","de"},{"to","a"},{"for","para"},{"in","en"},{"on","sobre"},{"under","debajo"},{"big","grande"},{"small","pequeno"},{"fast","rapido"},{"slow","lento"},{"new","nuevo"},{"old","viejo"},{"good","bueno"},{"bad","malo"},{"easy","facil"},{"difficult","dificil"},{"hot","caliente"},{"cold","frio"},{"happy","feliz"},{"sad","triste"},{"strong","fuerte"},{"weak","debil"},{"red","rojo"},{"blue","azul"},{"green","verde"},{"black","negro"},{"white","blanco"},{"one","uno"},{"two","dos"},{"three","tres"},{"four","cuatro"},{"five","cinco"},{"six","seis"},{"seven","siete"},{"eight","ocho"},{"nine","nueve"},{"ten","diez"},{"water","agua"},{"fire","fuego"},{"air","aire"},{"earth","tierra"},{"light","luz"},{"sound","sonido"},{"energy","energia"},{"force","fuerza"},{"speed","velocidad"},{"science","ciencia"},{"math","matematicas"},{"history","historia"},{"geography","geografia"},{"language","idioma"},{"word","palabra"},{"sentence","oracion"},{"question","pregunta"},{"answer","respuesta"},{"learn","aprender"},{"knowledge","conocimiento"},{"computer science","informatica"},{"cell","celula"},{"atom","atomo"},{"molecule","molecula"},{"oxygen","oxigeno"},{"carbon","carbono"},{"plant","planta"},{"animal","animal"},{"dog","perro"},{"cat","gato"},{"bird","ave"},{"fish","pez"},{"tree","arbol"},{"flower","flor"},{"school","escuela"},{"teacher","profesor"},{"student","alumno"},{"read","leer"},{"write","escribir"},{"speak","hablar"},{"listen","escuchar"},{"see","ver"},{"go","ir"},{"come","venir"},{"eat","comer"},{"drink","beber"},{"run","correr"},{"walk","caminar"},{"play","jugar"},{"make","hacer"},{"do","hacer"},{"know","saber"},{"think","pensar"},{"understand","entender"},{"remember","recordar"},{"forget","olvidar"},{"help","ayudar"},{"use","usar"},{"want","querer"},{"need","necesitar"},{"can","poder"},{"must","deber"},{"should","deberia"},{"learned","aprendido"},{"teacher","maestro"},{"world","mundo"},{"country","pais"},{"planet","planeta"},{"space","espacio"},{"science","ciencia"},{"technology","tecnologia"}
};
static const int DICT_COUNT=sizeof(dictionary)/sizeof(dictionary[0]);


static const char* lookupKnowledge(const char* q){
    static char line[ANSWER_MAX];
    static char result[ANSWER_MAX];
    if(!knowledgeOK) return 0;
    FILE* f=fopen(KNOWLEDGE_FILE,"rb");
    if(!f) return 0;
    while(fgets(line,sizeof(line),f)){
        char* sep=strchr(line,'|');
        if(!sep) continue;
        *sep=0;
        // Ignora la etiqueta [definition], [example], etc.; la clave real es el texto anterior.
        char* tag=strstr(line," [");
        if(tag) *tag=0;
        // Las fichas tienen claves sencillas sin acentos para funcionar con el teclado del DS.
        if(contains(q,line)){
            const char* a=sep+1;
            copyText(result,a,ANSWER_MAX);
            size_t n=strlen(result);
            while(n>0 && (result[n-1]=='\n'||result[n-1]=='\r')) result[--n]=0;
            fclose(f);
            return result;
        }
    }
    fclose(f);
    return 0;
}

static const char* translateWord(const char* raw){
    static char out[MSG_MAX];
    char q[INPUT_MAX+1]; normalize(raw,q,sizeof(q));
    if(strncmp(q,"traducir ",9)!=0) return 0;
    const char* w=q+9; while(*w==' ') w++;
    if(!*w) return "Usa: TRADUCIR palabra en ingles";
    for(int i=0;i<DICT_COUNT;i++) if(strcmp(w,dictionary[i].en)==0){ sprintf(out,"%s = %s",dictionary[i].en,dictionary[i].es); return out; }
    return "No tengo esa palabra en mi diccionario offline. Puedes ensenarmela con APRENDER.";
}

static const char* answer(const char* raw){
    static char out[ANSWER_MAX];
    char q[INPUT_MAX+1]; normalize(raw,q,sizeof(q));
    if(!q[0]) return "Escribe algo y te respondo.";

    if(strcmp(q,"xd")==0) return "La extraño mucho aaah me proyecto City Boy";
    if(strcmp(q,"42")==0) return "42. La respuesta clasica a la gran pregunta.";
    if(strcmp(q,"bruh")==0) return "bruh xd";
    if(contains(q,"quien creo esto")||contains(q,"quien creo a dsi ia")) return "DSi IA. Este proyecto fue creado para funcionar offline en Nintendo DS/DSi.";
    if(strcmp(q,"dsi")==0) return "Nintendo DSi: consola portatil de Nintendo con dos pantallas y pantalla tactil.";
    if(strcmp(q,"inteligente")==0){ detailed=!detailed; return detailed?"Modo INTELIGENTE activado. Dare respuestas mas desarrolladas.":"Modo INTELIGENTE desactivado. Volvemos a respuestas cortas."; }
    if(strcmp(q,"said")==0){ saidMode=!saidMode; return saidMode?"Modo Said activado. Voy a hablar mas informal.":"Modo Said desactivado."; }
    if(strcmp(q,"calculadora")==0||strcmp(q,"activa modo calculadora")==0){ calcMode=!calcMode; return calcMode?"Calculadora activada. Usa CALC 25+17 o CALC 12*3.":"Calculadora desactivada."; }
    if(strcmp(q,"guardar")==0){ saveData(); return storageOK?"Listo. Memoria y conversacion guardadas en DSiIA.dat.":"No pude acceder al almacenamiento."; }
    if(strcmp(q,"borrar conversacion")==0){ historyCount=0; scroll=0; saveData(); return "Conversacion borrada y guardado actualizado."; }
    if(strcmp(q,"borrar memoria")==0){ learnedCount=0; saveData(); return "Memoria aprendida borrada."; }
    if(strcmp(q,"estado memoria")==0){ sprintf(out,"Tengo %d mensajes y %d cosas aprendidas. Guardado: %s.",historyCount,learnedCount,storageOK?"SI":"NO"); return out; }
    if(contains(q,"aprender ")){
        const char* p=strstr(q,"aprender ")+9;
        const char* sep=strchr(p,'|');
        if(!sep) return "Usa: APRENDER pregunta | respuesta";
        char lq[96], la[MSG_MAX]; int n=(int)(sep-p); if(n>=95)n=95; memcpy(lq,p,n); lq[n]=0;
        while(n>0&&lq[n-1]==' ') lq[--n]=0;
        const char* aa=sep+1; while(*aa==' ') aa++;
        if(!lq[0]||!aa[0]) return "Falta la pregunta o la respuesta.";
        addLearned(lq,aa);
        return "Aprendido y guardado. No deberia perderse al reiniciar.";
    }
    if(calcMode && strncmp(q,"calc ",5)==0){
        double a,b; char op=0;
        if(sscanf(q+5,"%lf %c %lf",&a,&op,&b)==3){
            double r=0; bool ok=true;
            if(op=='+')r=a+b; else if(op=='-')r=a-b; else if(op=='*')r=a*b; else if(op=='/'&&b!=0)r=a/b; else ok=false;
            if(ok){ sprintf(out,"Resultado: %.6g",r); return out; }
        }
        return "Usa CALC numero operador numero. Ejemplo: CALC 12*4";
    }

    const char* tr=translateWord(raw); if(tr) return tr;
    const char* learnedAnswer=lookupLearned(q); if(learnedAnswer) return learnedAnswer;
    const char* knowledgeAnswer=lookupKnowledge(q); if(knowledgeAnswer){
        if(detailed){ snprintf(out,ANSWER_MAX,"%s\n\nModo INTELIGENTE: puedes pedir otro ejemplo o relacionarlo con otro tema.",knowledgeAnswer); return out; }
        copyText(out,knowledgeAnswer,ANSWER_MAX); return out;
    }
    for(int i=0;i<EXTRA_COUNT;i++) if(contains(q,extraKB[i].key)){ if(detailed){ sprintf(out,"%s\n\nModo INTELIGENTE: puedo ampliar la explicacion si haces otra pregunta sobre el mismo tema.",extraKB[i].answer); return out; } return extraKB[i].answer; }

    if(contains(q,"hola")||contains(q,"buenas")||contains(q,"hey")) return saidMode?"Que onda. Soy DSi IA, listo para darle.":"Hola. Soy DSi IA. Preguntame de lo que quieras aprender.";
    if(contains(q,"como estas")) return "Funcionando dentro de un Nintendo DS imaginario y bastante contento con eso xd.";
    if(contains(q,"gracias")) return "De nada. Para eso estoy.";
    if(contains(q,"quien eres")) return "Soy DSi IA: un asistente educativo offline hecho para Nintendo DS/DSi. No necesito Internet para responder mi base de conocimientos.";
    if(contains(q,"que puedes hacer")) return "Puedo explicar materias, resolver cuentas sencillas, calcular con CALC, recordar lo aprendido y guardar la conversacion.";
    if(strcmp(q,"ayuda")==0||strcmp(q,"comandos")==0) return "Comandos: INTELIGENTE, SAID, CALCULADORA, CALC 2+2, APRENDER pregunta | respuesta, TRADUCIR palabra, GUARDAR, ESTADO MEMORIA, BORRAR CONVERSACION, BORRAR MEMORIA.";

    for(int i=0;i<KB_COUNT;i++) if(contains(q,kb[i].key)){
        if(detailed){ sprintf(out,"%s\n\nPista: si quieres, preguntame por un ejemplo o escribe INTELIGENTE para cambiar de modo.",kb[i].answer); return out; }
        return kb[i].answer;
    }
    sprintf(out,"Todavia no tengo una respuesta especifica para eso. Puedes enseñarme con:\nAPRENDER pregunta | respuesta\nY quedara guardado.");
    return out;
}

static void drawTop(){
    consoleSelect(&topScreen); consoleClear();
    iprintf("DSi IA  |  %s | BASE %s\n",storageOK?"AUTO-GUARDADO ON":"GUARDADO OFF",knowledgeOK?"2+ MB":"OFF");
    iprintf("------------------------------\n");
    int start=historyCount-12-scroll; if(start<0)start=0; if(start>historyCount-1)start=historyCount-1;
    int shown=0;
    for(int i=start;i<historyCount && shown<12;i++,shown++){
        iprintf("%c: ",history[i].who=='U'?'T':'IA');
        iprintf("%.25s\n",history[i].text);
        if(strlen(history[i].text)>25) iprintf("   %.25s\n",history[i].text+25);
        if(strlen(history[i].text)>50) iprintf("   %.25s\n",history[i].text+50);
    }
    iprintf("\nToca flechas para ver historial.");
}

static void keyText(char* s){
    consoleSelect(&bottomScreen); iprintf("\x1b[1;0HEntrada: %-25s",s); }

static void drawKeyboard(){
    consoleSelect(&bottomScreen); consoleClear();
    iprintf("DSi IA - TECLADO TACTIL\n");
    iprintf("------------------------------\n");
    iprintf("Q W E R T Y U I O P\n");
    iprintf(" A S D F G H J K L\n");
    iprintf("  Z X C V B N M\n");
    iprintf("  1 2 3 4 5 6 7 8 9 0\n\n");
    iprintf("[ ESPACIO ] [ BORRAR ]\n");
    iprintf("[ MANDAR ] [ CAPS ]\n\n");
    iprintf("Toca una tecla. X/Y o flechas cambian historial.\n");
    keyText(input);
}

static char keyAt(int x,int y){
    // zonas tactiles amplias y separadas
    if(y>=18&&y<48){ const char* r="QWERTYUIOP"; int c=x/25; if(c<10)return r[c]; }
    if(y>=48&&y<78){ const char* r="ASDFGHJKL"; int c=(x-12)/27; if(c>=0&&c<9)return r[c]; }
    if(y>=78&&y<108){ const char* r="ZXCVBNM"; int c=(x-35)/27; if(c>=0&&c<7)return r[c]; }
    if(y>=108&&y<138){ const char* r="1234567890"; int c=x/25; if(c<10)return r[c]; }
    return 0;
}

static void addAnswerChunks(const char* text){
    if(!text || !text[0]) return;
    const char* p=text;
    int safety=0;
    while(*p && safety<12){
        char chunk[MSG_MAX];
        int len=0, lastSpace=-1;
        while(p[len] && len<MSG_MAX-1){
            if(p[len]==' ' && len>0) lastSpace=len;
            len++;
        }
        if(p[len]){
            if(lastSpace>0) len=lastSpace;
        }
        if(len<=0) len=MSG_MAX-1;
        memcpy(chunk,p,len); chunk[len]=0;
        addHistory('A',chunk);
        p += len;
        while(*p==' ') p++;
        safety++;
    }
}

static void sendInput(){
    if(inputLen<=0)return;
    input[inputLen]=0; char userCopy[MSG_MAX]; copyText(userCopy,input,MSG_MAX);
    addHistory('U',userCopy);
    const char* a=answer(input); addAnswerChunks(a);
    inputLen=0; input[0]=0; saveData(); drawTop(); drawKeyboard();
}

static void touchAction(int x,int y){
    if(y<18)return;
    char c=keyAt(x,y);
    if(c){ if(inputLen<INPUT_MAX){ if(caps)c=(char)toupper((unsigned char)c); input[inputLen++]=c; input[inputLen]=0; keyText(input); } return; }
    if(y>=138&&y<163){ if(x<115){ if(inputLen<INPUT_MAX){input[inputLen++]=' ';input[inputLen]=0;} } else if(x<210){ if(inputLen>0)input[--inputLen]=0;} else { sendInput(); } keyText(input); return; }
    if(y>=163){ if(x<128){ sendInput(); } else { caps=!caps; } keyText(input); }
}

int main(void){
    defaultExceptionHandler();
    videoSetMode(MODE_0_2D); videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
    consoleInit(&topScreen,0,BgType_Text4bpp,BgSize_T_256x256,31,0,true,true);
    consoleInit(&bottomScreen,0,BgType_Text4bpp,BgSize_T_256x256,31,0,true,true);
    knowledgeOK=nitroFSInit(NULL);
    storageOK=fatInitDefault();
    loadData();
    if(historyCount==0){ addHistory('A',"Hola. Soy DSi IA. Toca el teclado de abajo para escribir."); addHistory('A',"Mis comandos: INTELIGENTE, CALC, APRENDER, GUARDAR y ESTADO MEMORIA."); saveData(); }
    drawTop(); drawKeyboard();

    while(pmMainLoop()){
        scanKeys(); touchPosition t; touchRead(&t);
        int kd=keysDown();
        if(kd&KEY_START) break;
        if(kd&KEY_LEFT){ if(scroll<historyCount-12)scroll++; drawTop(); }
        if(kd&KEY_RIGHT){ if(scroll>0)scroll--; drawTop(); }
        if(kd&KEY_B){ if(inputLen>0)input[--inputLen]=0; keyText(input); }
        if(kd&KEY_A)sendInput();
        if(kd&KEY_X){ sendInput(); }
        if(kd&KEY_Y){ saveData(); }
        if(kd&KEY_TOUCH) touchAction(t.px,t.py);
        swiWaitForVBlank();
    }
    saveData();
    return 0;
}
