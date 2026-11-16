// Example 2: functions, locals, collision between two entities
GAME Arena {

  FUNCTION calculateDamage(attack: INTEGER, defense: INTEGER): INTEGER {
    VAR dmg: INTEGER = attack - defense;
    IF dmg < 0 { RETURN 0; }
    RETURN dmg;
  }

  ENTITY Enemy {
    PROPERTY attack: INTEGER = 30;
    ON START  { position = VECTOR2(100, 0); }
    ON UPDATE { MOVE LEFT BY 5; }
  }

  ENTITY Player {
    PROPERTY health: INTEGER = 100;
    PROPERTY defense: INTEGER = 10;
    ON COLLISION WITH Enemy {
      health = health - calculateDamage(Enemy.attack, defense);
      PRINT "Player hit! health is now";
      PRINT health;
      IF health <= 0 { DESTROY SELF; }
    }
    ON DEATH { PRINT "Player destroyed"; }
  }
}
