package uk.co.busydoingnothing.prevo;

public class Hats
{
  public static String removeHats (CharSequence string)
  {
    StringBuilder result = new StringBuilder ();

    for (int i = 0; i < string.length (); i++)
      {
        if (i + 1 < string.length () &&
            (string.charAt (i + 1) == 'x' ||
             string.charAt (i + 1) == 'X'))
          {
            switch (string.charAt (i))
              {
              case 'c':
                result.append ('ĉ');
                break;
              case 'C':
                result.append ('Ĉ');
                break;
              case 'g':
                result.append ('ĝ');
                break;
              case 'G':
                result.append ('Ĝ');
                break;
              case 'h':
                result.append ('ĥ');
                break;
              case 'H':
                result.append ('Ĥ');
                break;
              case 'j':
                result.append ('ĵ');
                break;
              case 'J':
                result.append ('Ĵ');
                break;
              case 's':
                result.append ('ŝ');
                break;
              case 'S':
                result.append ('Ŝ');
                break;
              case 'u':
                result.append ('ŭ');
                break;
              case 'U':
                result.append ('Ŭ');
                break;
              default:
                result.append (string.subSequence (i, i + 2));
                break;
              }
            i++;
          }
        else
          result.append (string.charAt (i));
      }

    return result.toString ();
  }
}
