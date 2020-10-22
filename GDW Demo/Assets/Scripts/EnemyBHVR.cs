using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class EnemyBHVR : MonoBehaviour
{
    public float speed;
    public GameObject player;
    public GameObject powerup1;
    public GameObject powerup2;

    // Start is called before the first frame update
    void Start()
    {
        player = GameObject.FindWithTag("Player");
    }

    // Update is called once per frame
    void Update()
    {
        Physics.IgnoreCollision(powerup1.GetComponent<Collider>(), GetComponent<Collider>());
        Physics.IgnoreCollision(powerup2.GetComponent<Collider>(), GetComponent<Collider>());
        transform.LookAt(player.transform.position);
        Vector3 direction = Vector3.Normalize(player.transform.position - transform.position);
        transform.position += direction * speed;
    }

    private void OnCollisionEnter(Collision collision)
    {
        if (collision.gameObject.CompareTag("Bullet"))
        {
            float rand = Random.value;
            if (rand <= 0.3f)
            {
                GameObject p1 = Instantiate(powerup1, transform.position, transform.rotation);
                Destroy(gameObject);
                Destroy(collision.gameObject);
            }
            else if (rand <= 0.6f && rand > 0.3f)
            {
                GameObject p2 = Instantiate(powerup2, transform.position, transform.rotation);
                Destroy(gameObject);
                Destroy(collision.gameObject);
            }
            else if (rand > 0.6f)
            {
                Destroy(gameObject);
                Destroy(collision.gameObject);
            }
        }
        else if (collision.gameObject.CompareTag("Player"))
        {
            Destroy(collision.gameObject);
        }
    }
}
