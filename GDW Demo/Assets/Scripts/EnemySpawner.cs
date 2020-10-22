using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class EnemySpawner : MonoBehaviour
{
    public GameObject enemy;
    public float spawnCD;
    public float spawncount;
    public float spawnbuffer;
    private float timer;
    private float spawnsleft;
    private float buffertimer;

    // Start is called before the first frame update
    void Start()
    {
        timer = spawnCD;
        spawnsleft = spawncount;
        buffertimer = spawnbuffer;
    }

    // Update is called once per frame
    void Update()
    {
           if (timer <= 0)
           {
            if (buffertimer <= 0)
            {
                spawnsleft--;
                GameObject enemyclone;
                enemyclone = Instantiate(enemy, transform.position, transform.rotation);
                buffertimer = spawnbuffer;
            }
            else
            {
                buffertimer -= Time.deltaTime;
            }
           
        }
           else
           { 
               timer -= Time.deltaTime; 
           }
           
           if (spawnsleft == 0)
           {
               spawnsleft = spawncount;
               timer = spawnCD;
           }
    }
}
