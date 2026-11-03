# Implementação 5 - Sincronização do Metrô

- Aluno: Marcus Vinícius Pereira Barbosa
- E-mail: mvpb@cesar.school

## Descrição

Sincronização entre vagões de metrô e passageiros, cada um controlado por uma thread, usando apenas mutex e variáveis de condição da biblioteca pthread, não podendo usar semáforos e sem busy-wait. A struct `station` usa um mutex, duas variáveis de condição (passageiros e vagão) e três contadores (passageiros em espera, liberados que ainda não sentaram e os assentos livres).

## Arquivos

- `sync_car.c`: implementação de `station_init`, `station_load_car`, `station_wait_for_car` e `station_on_board`.
- `sync_car.h`: definição da `struct station` e protótipos das funções existentes (no .c).
- `sync_car-runner.c`: programa de teste fornecido pelo professor, com a main nele.
- `sync_car-testes.c`: bateria de testes extra, com a sua própria main. Tem 21 cenários (C01 a C21), incluindo casos limite (ex.: `INT_MAX` assentos, 5000 passageiros) e casos inválidos (ex.: `count` negativo, `on_board` chamado a mais), além de testes aleatórios com verificação do resultado e verificação de busy-wait.
- `ruido.h`: cabeçalho usado só pelo alvo `testes-ruido`. Injeta `sched_yield` e pequenas pausas aleatórias nas chamadas de mutex e condição, sem alterar o `sync_car.c`, para forçar trocas de thread.
- `Makefile`: compilação, execução dos testes e limpeza.
- `evidencias.log`: log do terminal (gerado com `script -a`) com compilações, correções e testes.

## Como compilar

```
make
```
Gera o executável `sync_car`.

## Como executar e testar

Teste do professor:
```
make test
```
Compila (se necessário) e executa o runner. Se tudo estiver correto, a saída termina com `Looks good!`.

Bateria de testes extra (leva cerca de 20 segundos):
```
make testes
```
Compila e executa o `sync_car-testes.c`. Se tudo estiver correto, a saída termina com `RESULTADO FINAL: TODOS OS TESTES PASSARAM`. Cada cenário mostra `PASSOU` ou `FALHOU`, e um deadlock aparece como `FALHOU (TRAVOU / timeout)`.

Bateria extra com ruído de escalonamento:
```
make testes-ruido
```
Roda os mesmos testes com o `sync_car.c` compilado junto com o `ruido.h`.

Para rodar só uma parte (depois de `make sync_car_testes`):
```
./sync_car_testes casos
./sync_car_testes aleatorios 50
```

## Como limpar

```
make clean
```
Remove os executáveis `sync_car`, `sync_car_testes` e `testes_ruido`, e o objeto `sync_car_ruido.o`.

## Sistema operacional

Implementado e testado em Ubuntu 24.04 (Linux), via WSL no Windows, compilado com gcc.
