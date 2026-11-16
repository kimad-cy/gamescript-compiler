// Example 5: keyboard input, movement, spawning, vectors, rules
// Try:  python3 ex5_shooter.py --keys "2:SPACE,4:LEFT" --frames 40
GAME Shooter {

  ENTITY Bullet {
    PROPERTY speed: FLOAT = 8.5;
    ON UPDATE { MOVE UP BY speed; }
  }

  ENTITY Enemy {
    PROPERTY hits: INTEGER = 0;
    ON START { position = VECTOR2(100, 300); }
    ON COLLISION WITH Bullet {
      hits = hits + 1;
      PRINT "enemy hit!";
      DESTROY Bullet;
      PLAY SOUND "explosion.wav";
    }
  }

  ENTITY Player {
    PROPERTY speed: INTEGER = 5;
    ON START { position = VECTOR2(100, 400); }
    ON KEY_PRESS "LEFT"  { MOVE LEFT BY speed; }
    ON KEY_PRESS "RIGHT" { MOVE RIGHT BY speed; }
    ON KEY_PRESS "SPACE" {
      SPAWN Bullet AT position + VECTOR2(0, -10);
      PLAY SOUND "shoot.wav";
    }
  }

  RULE Victory { IF Enemy.hits >= 1 { WIN; } }
}
