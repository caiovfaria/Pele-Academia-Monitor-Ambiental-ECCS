# Monitor Ambiental de Treinamento — Pelé Academia (Resende)
### Projeto ECCS — Sprint 3 — Edge Computing & Computer Systems

MEMBROS DO GRUPO:
João Thees Castro Santiago/
Caio Viana de Faria/
Anna Júlia Elias Andrade/
Clara Diel Gama Secco/
Arthur Alen Amorelli Pereira

## Descrição do sistema

Protótipo baseado em ESP32 para monitorar condições ambientais (temperatura e
umidade) nos campos abertos de treinamento da Pelé Academia. O sistema calcula
localmente um **Índice de Calor** (Heat Index) e classifica o risco térmico
para atividade física, exibindo o resultado em um display LCD 16x2 e enviando
o histórico para a plataforma ThingSpeak.

Como os campos são abertos, a exposição direta ao sol e à umidade tem impacto
real sobre o risco de fadiga térmica e desidratação dos atletas — por isso o
índice de calor (que combina as duas variáveis) foi escolhido como indicador
de risco, em vez de limites fixos e isolados de temperatura ou umidade.

## Arquitetura: Edge vs Cloud

### Operações realizadas na borda (ESP32) — Edge Computing
- Aquisição periódica de temperatura e umidade via sensor DHT22 (a cada 5s)
- Cálculo da **média móvel** das últimas 5 leituras (suaviza ruído/outliers)
- Cálculo do **Índice de Calor** (fórmula de Rothfusz, modelo NWS)
- **Classificação do risco térmico** em 4 níveis: Normal, Atenção, Perigo,
  Perigo Extremo
- **Apresentação imediata** dos dados e do status na IHM local (display LCD)

Essas operações acontecem de forma autônoma no dispositivo: mesmo sem
conexão Wi-Fi, o treinador consegue ver as condições atuais e o nível de
risco diretamente no display, sem depender da nuvem.

### Operações realizadas na nuvem (ThingSpeak) — Cloud
- Armazenamento histórico das médias de temperatura, umidade, índice de
  calor e nível de risco
- Geração de **gráficos** de acompanhamento ao longo do tempo
- Disponibilização de um **canal público** para consulta remota das
  condições dos campos

## Hardware (simulado no Wokwi)
- ESP32 DevKit
- Sensor DHT22 (temperatura + umidade)
- Display LCD 16x2 (I2C)

## Como rodar no Wokwi
1. Acesse o link do projeto Wokwi: https://wokwi.com/projects/474902121286379521
2. Clique em "Start Simulation"
3. Aguarde a conexão Wi-Fi (rede `Wokwi-GUEST`, com acesso real à internet)
4. Acompanhe as leituras no display virtual e no Serial Monitor
5. Os dados são enviados automaticamente ao ThingSpeak a cada ~16s

## Configuração do ThingSpeak
1. Criar uma conta em https://thingspeak.com
2. Criar um novo canal com 4 campos:
   - **Field 1:** Temperatura média (°C)
   - **Field 2:** Umidade média (%)
   - **Field 3:** Índice de calor (°C)
   - **Field 4:** Nível de risco (0=Normal, 1=Atenção, 2=Perigo, 3=Perigo Extremo)
3. Copiar a **Write API Key** do canal e colar em `TS_API_KEY` no `sketch.ino`
4. Em **Channel Settings**, marcar o canal como **público**
5. Copiar o link público do canal para a entrega

## Classificação de risco (Índice de Calor) e relação com o treino esportivo

O índice de calor combina temperatura e umidade em um único valor que reflete
a sensação térmica real sentida pelo corpo — mais relevante para o risco de
treino em campo aberto do que analisar cada variável isoladamente. Como os
campos da Pelé Academia são abertos, sem cobertura ou climatização, os atletas
ficam diretamente expostos às condições medidas pelo sensor.

| Índice de Calor | Nível          | Interpretação para o treino                                      |
|------------------|----------------|--------------------------------------------------------------------|
| < 32°C           | Normal         | Condições seguras para treino em intensidade normal               |
| 32°C – 41°C      | Atenção        | Reforçar hidratação e monitorar sinais de fadiga nos atletas      |
| 41°C – 54°C      | Perigo         | Risco real de fadiga térmica; reduzir intensidade e aumentar pausas |
| ≥ 54°C           | Perigo Extremo | Risco de insolação; recomenda-se suspender ou remanejar o treino  |

Na prática, isso significa que o sistema não apenas informa a temperatura do
ambiente, mas **traduz esse dado em uma recomendação acionável** para quem
está coordenando o treino: um valor de 35°C de temperatura seca pode parecer
tolerável, mas se combinado a 80% de umidade o índice de calor calculado
ultrapassa a faixa de "Perigo", indicando que o corpo tem dificuldade de
dissipar calor por suor — situação muito comum em dias úmidos no Rio de
Janeiro/Resende, especialmente em treinos no período da tarde.

## Links da entrega
- Repositório GitHub: **[INSERIR LINK AQUI]**
- Projeto Wokwi: https://wokwi.com/projects/474902121286379521
- Canal público ThingSpeak: https://thingspeak.mathworks.com/channels/3496483

## Estrutura do repositório
```
.
├── sketch.ino        # código-fonte do ESP32
├── diagram.json       # diagrama de circuito do Wokwi
├── libraries.txt       # bibliotecas usadas na simulação
└── README.md          # este arquivo
```
