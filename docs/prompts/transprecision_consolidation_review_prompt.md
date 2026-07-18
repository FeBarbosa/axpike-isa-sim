# Transprecision Consolidation Review Prompt

Use este prompt para iniciar um novo chat focado em consolidar, revisar,
documentar e estudar a implementação atual da transprecisão no AxPIKE antes de
iniciar a próxima política experimental.

```text
Leia AGENTS.md, docs/agents/development_agent.md e
docs/agents/research_documentation_agent.md.

Quero consolidar a implementação atual da transprecisão no AxPIKE antes de
iniciar a política de promoção/demoção baseada em bits menos significativos
da mantissa.

Parte importante deste trabalho é melhorar meu entendimento técnico e científico
da implementação. O processo deve funcionar como um estudo guiado: reconstrua as
decisões feitas, explique como a transprecisão foi implementada no código, como
ela foi validada, quais métricas foram incorporadas ao AxPIKE e como cada uma
dessas partes sustenta ou limita o experimento. Evite apenas resumir o estado
final; ajude-me a ser capaz de explicar a implementação e suas validações.

Faça essa reconstrução de maneira iterativa, em pequenos passos. A cada passo,
explique um bloco limitado da implementação ou da validação, conecte esse bloco
ao objetivo científico da transprecisão e proponha um pequeno questionário para
verificar meu entendimento antes de avançar. Use as minhas respostas para ajustar
o nível de detalhe, corrigir lacunas conceituais e decidir o próximo bloco de
estudo.

Objetivos:
1. revisar o estado atual da implementação;
2. mapear os arquivos modificados e responsabilidades;
3. listar as decisões de projeto já tomadas;
4. listar validações executadas e o que cada uma prova;
5. identificar limitações conhecidas;
6. preparar um resumo técnico sucinto para meus orientadores;
7. organizar próximos passos até a submissão do artigo em 7 de agosto de 2026.

Não implemente nada inicialmente. Primeiro avalie o repositório e produza um
plano de documentação e validação.

Estado conhecido:
- transprecision tags foram adicionadas ao estado dos registradores FP;
- loads/escritas arquiteturais classificam o valor escrito;
- operações FP classificam operandos e usam o maior tipo efetivo;
- resultados são reclassificados no write-back;
- especiais têm política definida: NaN/Inf de operação usam tipo efetivo,
  NaN/Inf externos usam tipo arquitetural, +/-0 usa menor tipo disponível;
- contadores de transprecisão foram adicionados ao CSV;
- LeNet executou e gerou AxPIKE_transprecision_*.csv;
- fcvt_d_s/fcvt_s_d foram ajustadas para contar pelo tipo efetivo do operando
  fonte.
```

Estrutura esperada para o e-mail aos orientadores:

```text
Assunto: Status da implementação de transprecisão no AxPIKE

1. Objetivo da implementação
2. O que já foi incorporado ao AxPIKE
3. Métricas e CSVs gerados
4. Validações realizadas
5. Resultado preliminar com LeNet
6. Limitações atuais
7. Próximo experimento: política baseada em bits da mantissa
8. Cronograma até 7 de agosto
```
