# Projeto de Hardware

* **Qual hardware para troca de informações você irá desenvolver ou aprimorar utilizando conceitos de IoT?**

    Irei utilizar um ESP32 (numa placa NodeMCU) para coletar o MAC Address de dispositivos BLE na região. Talvez seja necessário utilizar também um ESP8266 para conexão com WiFi, pois não tenho certeza se é possível utilizar o Bluetooth e o WiFi ao mesmo tempo no ESP32.

* **Descreva o que é entrada (sensores/dados) e o que é saída (dados/informações).**
  * Entrada: Beacons BLE.
  * Saída: Presença ou não de indivíduos

* **Descreva o filtro que será aplicado nos dados, pois o dado de entrada não deve ser igual à informação da saída.**
  * Será filtrada a intensidade do sinal do pacote, sendo que intensidades abaixo de um limiar serão descartadas.
  * Além disso, talvez seja necessário fazer uma média de algumas amostras para melhorar a estabilidade da medição (ex: se um pacote for perdido, etc...)

* **Descreva como o hardware irá se comunicar. Se será por meio de alguma API ou outra tecnologia.**
  * Será utilizado MQTT para comunicação com um servidor do Home Assistant rodando na rede local.


## Informações adicionais
### Ignorando linhas no commit
https://stackoverflow.com/questions/16244969/how-to-tell-git-to-ignore-individual-lines-i-e-gitignore-for-specific-lines-of
`git config filter.gitignore.clean "sed '/#gitignore/d'"`
`git config filter.gitignore.smudge cat`
