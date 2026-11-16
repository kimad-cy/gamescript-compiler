// Example 2 from the spec: collision, functions, locals
GAME Arena {
  FUNCTION calculateDamage(attack: INTEGER, defense: INTEGER): INTEGER {
    VAR dmg: INTEGER = attack - defense;
    IF dmg < 0 { RETURN 0; }
    RETURN dmg;
  }
  ENTITY Enemy { PROPERTY attack: INTEGER = 30; }
  ENTITY Player {
    PROPERTY health: INTEGER = 100;
    PROPERTY speed: FLOAT = 5.5;
    PROPERTY name: STRING = "Hero \"One\"\n";
    ON COLLISION WITH Enemy {
      health = health - calculateDamage(Enemy.attack, 10); /* block
                                                              comment */
      IF health <= 0 { DESTROY SELF; }
    }
  }
}
