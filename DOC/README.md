## Mejoras futuras
- Que los nodos no deban de estar en la misma red que el servidor.
- Montar el servidor en un host cloud.
- Diseñarle acoples en 3D del nodo principal y de su extensión para el sensor s2.
- Agregarle una instancia de persistencia de datos, módulo microSD para que almacene las lecturas, pregunte si las lecturas guardadas respecto a las del servidor están actualizadas o deberá de enviarlas.
- Agregar en grafana la posibilidad de exportar todos esos datos que visualiza en un archibo .xlsx o .csv para análisis fuera de grafana.
- Agregar un botón de configuración (que permita ponerlo en modo AP y cambiar credenciales manualmente de red y mqtt).
- Configurar parámetros del nodo por mqtt (nodo id, cliente, etc).

## Faltó realizar pruebas
- Recepción de señal Wi-Fi en entornos industriales o de ruido electromagnético (para disernir si agregar antena externa o no).
- Comprobar ruidos EM que se colen en el cable del sensor s2.
