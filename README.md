# Implementação 5 - Sincronização do Metrô

- Aluno: Marcus Vinícius Pereira Barbosa
- E-mail: mvpb@cesar.school

## Descrição

Sincronização entre vagões de metrô e passageiros, cada um controlado por uma thread, usando apenas mutex e variáveis de condição da biblioteca pthread, não podendo usar semáforos e sem busy-wait. A struct `station` usa um mutex, duas variáveis de condição (passageiros e vagão) e três contadores (passageiros em espera, liberados que ainda não sentaram e os assentos livres).

## Arquivos

- `sync_car.c`: implementação de `station_init`, `station_load_car`, `station_wait_for_car` e `station_on_board`.
- `sync_car.h`: definição da `struct station` e protótipos das funções existentes (no .c).
- `sync_car-runner.c`: programa de teste fornecido, com a main nele.
- `Makefile`: compilação, execução do teste e limpeza.

## Como compilar

```
make
```
Gera o executável `sync_car`.

## Como executar e testar

```
make test
```
Compila (se necessário) e executa o runner. Se tudo estiver correto, a saída termina com `Looks good!`.

## Como limpar

```
make clean
```
Remove o executável `sync_car`.

## Sistema operacional

Implementado e testado em Linux na versão 24.04, via WSL no Windows.
