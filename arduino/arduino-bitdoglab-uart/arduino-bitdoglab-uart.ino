void setup() {
  // Inicializa a comunicação serial com a BitDogLab
  Serial.begin(9600);
}

void loop() {
  // Envia a mensagem de teste
  Serial.println("Ola BitDogLab!");
  
  // Aguarda 2 segundos antes de enviar a próxima mensagem
  delay(2000); 
}