# Algoritmo do Banqueiro (banker)

Implementação 5 — Infraestrutura de Software (CESAR School).
Implementa o algoritmo do banqueiro (Silberschatz et al., *Operating System Concepts*, 10ª ed., p. P-45)
com número de clientes e de recursos definidos em tempo de execução.

## Compilar e limpar
```
make          # gera o executável ./banker
make clean    # remove ./banker e result.txt
make test     # roda tests/run_tests.sh (20 casos) e regrava evidencias.log
```
Requisitos: gcc e make em Linux, Unix ou macOS.

## Executar
```
./banker 10 5 7
```
Cada argumento é o total de instâncias de um tipo de recurso (aqui, 3 recursos).
O programa lê, no diretório atual:

| Arquivo | Formato |
|---|---|
| `customer.txt` | uma linha por cliente, valores separados por vírgula (`7,5,3`) |
| `commands.txt` | `RQ i r0 r1 ...`, `RL i r0 r1 ...` e `*` |

e grava o resultado em `result.txt`.

## Regras de um RQ (nesta ordem)
1. pedido > need do cliente → `denied because exceed its maximum allocation`
2. pedido > available → `The resources <available> are not enough to customer i request ...`
3. alocação tentativa + `is_safe()`; se inseguro, faz rollback → `denied because result in an unsafe state`
4. caso contrário → `Allocate to customer i the resources ...`

Extensão: um `RL` maior que o alocado é recusado com `denied because exceed its allocation`.

## Tratamento de erros
O programa encerra com código 1, mensagem `Erro: ...` no stderr, sem `result.txt` parcial e sem vazar
memória quando: faltam argumentos ou são inválidos; o número de argumentos difere do número de colunas do
`customer.txt` ou de valores do `commands.txt`; algum arquivo não existe/não abre/está vazio; ou o formato
está errado (valor não numérico ou negativo, comando desconhecido, cliente inexistente, `*` com parâmetros).
Todo o `commands.txt` é validado **antes** de qualquer comando ser executado.

## Estrutura
```
banker.c  Makefile  README.md  evidencias.log  relatorio.pdf
tests/run_tests.sh  tests/casos/<caso>/{args,customer.txt,commands.txt,esperado.txt}
```
