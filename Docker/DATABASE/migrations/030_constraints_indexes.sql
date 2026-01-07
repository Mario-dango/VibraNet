 
-- FKs (separadas para claridad)
ALTER TABLE usuario
  ADD CONSTRAINT fk_usuario_cliente
  FOREIGN KEY (cliente_id) REFERENCES cliente(cliente_id)
  ON UPDATE CASCADE ON DELETE RESTRICT;

ALTER TABLE sistema
  ADD CONSTRAINT fk_sistema_cliente
  FOREIGN KEY (cliente_id) REFERENCES cliente(cliente_id)
  ON UPDATE CASCADE ON DELETE RESTRICT;

ALTER TABLE usuario_sistema
  ADD CONSTRAINT fk_us_usuario
  FOREIGN KEY (usuario_id) REFERENCES usuario(usuario_id)
  ON UPDATE CASCADE ON DELETE CASCADE,
  ADD CONSTRAINT fk_us_sistema
  FOREIGN KEY (sistema_id) REFERENCES sistema(sistema_id)
  ON UPDATE CASCADE ON DELETE CASCADE;

ALTER TABLE nodo
  ADD CONSTRAINT fk_nodo_sistema
  FOREIGN KEY (sistema_id) REFERENCES sistema(sistema_id)
  ON UPDATE CASCADE ON DELETE RESTRICT;

ALTER TABLE mediciones
  ADD CONSTRAINT fk_med_nodo
  FOREIGN KEY (nodo_id) REFERENCES nodo(nodo_id)
  ON UPDATE CASCADE ON DELETE RESTRICT;
